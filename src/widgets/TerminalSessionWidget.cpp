#include "TerminalSessionWidget.h"

#include "AppSettings.h"
#include "ErrorNotifier.h"
#include "Logging.h"
#include "SshWorker.h"
#include "TunnelStore.h"

#include <QAbstractButton>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <qtermwidget.h>

#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr int kMinTerminalCols = 2;
constexpr int kMinTerminalRows = 2;
constexpr int kMaxPendingDisplayBytes = 1024 * 1024;
constexpr int kResizeDebounceMs = 80;
constexpr int kFlushRetryMs = 16;
constexpr int kDefaultPtyCols = 80;
constexpr int kDefaultPtyRows = 24;

QString sanitizeFileBaseName(const QString &name)
{
    QString base = name.trimmed();
    if (base.isEmpty()) {
        return QStringLiteral("session");
    }
    base.replace(QLatin1Char('/'), QLatin1Char('-'));
    base.replace(QLatin1Char('\\'), QLatin1Char('-'));
    return base;
}
}

TerminalSessionWidget::TerminalSessionWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_term = new QTermWidget(0, this);
    m_term->setTerminalSizeHint(false);
    applySettings();
    m_term->installEventFilter(this);
    layout->addWidget(m_term, 1);

    m_overlay = new QLabel(this);
    m_overlay->setAlignment(Qt::AlignCenter);
    m_overlay->setWordWrap(true);
    layout->addWidget(m_overlay);

    m_resizeDebounce = new QTimer(this);
    m_resizeDebounce->setSingleShot(true);
    m_resizeDebounce->setInterval(kResizeDebounceMs);
    connect(m_resizeDebounce, &QTimer::timeout, this, &TerminalSessionWidget::syncPtySize);

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(kFlushRetryMs);
    connect(m_flushTimer, &QTimer::timeout, this, &TerminalSessionWidget::flushPendingDisplay);

    m_term->hide();
    showDisconnectedState();

    // PMF connect fails for sendData(const char*,int) on QTermWidget ("signal not found").
    connect(m_term, SIGNAL(sendData(const char*,int)),
            this, SLOT(onSendData(const char*,int)));
}

TerminalSessionWidget::~TerminalSessionWidget()
{
    teardownPtyBridge();
    shutdownWorker();
    clearSecret();
}

void TerminalSessionWidget::start(const Connection &connection, const QString &secret)
{
    if (m_state == State::Connecting) {
        return;
    }

    m_connection = connection;
    m_secret = secret;
    m_displayName = connection.displayText();
    beginConnect();
}

void TerminalSessionWidget::copySelection()
{
    if (!m_term) {
        return;
    }
    m_term->copyClipboard();
}

void TerminalSessionWidget::pasteClipboard()
{
    if (!m_term) {
        return;
    }
    m_term->pasteClipboard();
}

void TerminalSessionWidget::toggleSearch()
{
    if (!m_term) {
        return;
    }
    m_term->toggleShowSearchBar();
}

void TerminalSessionWidget::clearScreen()
{
    if (!m_term) {
        return;
    }
    m_term->clear();
}

void TerminalSessionWidget::saveLog()
{
    if (!m_term) {
        return;
    }

    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(documents).filePath(
        sanitizeFileBaseName(m_displayName) + QStringLiteral(".log"));

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Log"),
        suggested,
        tr("Log files (*.log *.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ErrorNotifier::notify(
            this,
            tr("Save Log"),
            tr("Could not write log file:\n%1").arg(file.errorString()),
            ErrorNotifier::Level::Warning);
        return;
    }

    m_term->saveHistory(&file);
    file.close();
    emit statusMessage(tr("Saved log: %1").arg(path));
}

void TerminalSessionWidget::saveScreenshot()
{
    if (!m_term) {
        return;
    }

    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(documents).filePath(
        sanitizeFileBaseName(m_displayName) + QStringLiteral(".png"));

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Screenshot"),
        suggested,
        tr("PNG images (*.png);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    const QPixmap pixmap = m_term->grab();
    if (pixmap.isNull() || !pixmap.save(path, "PNG")) {
        ErrorNotifier::notify(
            this,
            tr("Save Screenshot"),
            tr("Could not save screenshot to:\n%1").arg(path),
            ErrorNotifier::Level::Warning);
        return;
    }

    emit statusMessage(tr("Saved screenshot: %1").arg(path));
}

void TerminalSessionWidget::disconnectSession()
{
    if (m_state != State::Connected && m_state != State::Connecting) {
        return;
    }

    shutdownWorker();
    teardownPtyBridge();
    showDisconnectedState();
    setState(State::Disconnected);
    emit statusMessage(tr("Disconnected: %1").arg(m_displayName));
    emit sessionDisconnected();
}

void TerminalSessionWidget::reconnect()
{
    if (m_state == State::Connecting) {
        return;
    }
    if (m_connection.id.isNull()) {
        return;
    }

    beginConnect();
}

void TerminalSessionWidget::applySettings()
{
    if (!m_term) {
        return;
    }

    const auto &settings = AppSettings::instance();
    m_term->setTerminalFont(settings.terminalFont());
    m_term->setColorScheme(settings.colorScheme());
    m_term->setHistorySize(settings.historySize());
    m_term->setBlinkingCursor(settings.cursorBlink());
    m_term->setConfirmMultilinePaste(settings.confirmMultilinePaste());

    using Shape = QTermWidget::KeyboardCursorShape;
    Shape shape = Shape::BlockCursor;
    switch (settings.cursorShape()) {
    case 1:
        shape = Shape::UnderlineCursor;
        break;
    case 2:
        shape = Shape::IBeamCursor;
        break;
    default:
        shape = Shape::BlockCursor;
        break;
    }
    m_term->setKeyboardCursorShape(shape);

    // Font / margin changes alter cols×rows without a QEvent::Resize — force PTY sync.
    m_lastCols = 0;
    m_lastRows = 0;
    schedulePtySizeSync();
}

QString TerminalSessionWidget::displayName() const
{
    return m_displayName;
}

QUuid TerminalSessionWidget::connectionId() const
{
    return m_connection.id;
}

Connection TerminalSessionWidget::connection() const
{
    return m_connection;
}

TerminalSessionWidget::State TerminalSessionWidget::sessionState() const
{
    return m_state;
}

bool TerminalSessionWidget::isSftpAvailable() const
{
    return m_sftpAvailable;
}

QString TerminalSessionWidget::sftpUnavailableReason() const
{
    return m_sftpUnavailableReason;
}

void TerminalSessionWidget::listDirectory(const QString &path)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path]() {
            worker->listDirectory(path);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::createDirectory(const QString &path)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path]() {
            worker->createDirectory(path);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::renamePath(const QString &from, const QString &to)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, from, to]() {
            worker->renamePath(from, to);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::removePath(const QString &path, bool recursive)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path, recursive]() {
            worker->removePath(path, recursive);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::uploadFiles(const QStringList &localPaths, const QString &remoteDir)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, localPaths, remoteDir]() {
            worker->uploadFiles(localPaths, remoteDir);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::uploadFileTo(const QString &localPath, const QString &remotePath)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, localPath, remotePath]() {
            worker->uploadFileTo(localPath, remotePath);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::downloadPaths(const QStringList &remotePaths, const QString &localDir)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, remotePaths, localDir]() {
            worker->downloadPaths(remotePaths, localDir);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::canonicalizePath(const QString &path)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path]() {
            worker->canonicalizePath(path);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::cancelTransfer()
{
    if (m_worker == nullptr) {
        return;
    }
    // Direct call is safe: cancelTransfer only sets an atomic flag.
    m_worker->cancelTransfer();
}

void TerminalSessionWidget::startTunnel(const TunnelDefinition &def)
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, def]() {
            worker->startTunnel(def);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::stopTunnel(const QUuid &tunnelId)
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, tunnelId]() {
            worker->stopTunnel(tunnelId);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::stopAllTunnels()
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker]() {
            worker->stopAllTunnels();
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::startEnabledTunnels()
{
    if (m_state != State::Connected || m_worker == nullptr) {
        return;
    }

    const QList<TunnelDefinition> tunnels = TunnelStore::loadForConnection(m_connection.id);
    for (const TunnelDefinition &tunnel : tunnels) {
        if (tunnel.enabled) {
            startTunnel(tunnel);
        }
    }
}

bool TerminalSessionWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_term && event->type() == QEvent::Resize) {
        // Debounce: fullscreen emits many intermediate sizes; sync once settled.
        schedulePtySizeSync();
    }
    return QWidget::eventFilter(watched, event);
}

void TerminalSessionWidget::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit sessionStateChanged(m_state);
}

void TerminalSessionWidget::beginConnect()
{
    shutdownWorker();
    m_shuttingDown = false;
    m_sftpAvailable = false;
    m_sftpUnavailableReason.clear();

    showConnectingState();
    setState(State::Connecting);
    emit statusMessage(tr("Connecting to %1…").arg(m_displayName));

    m_thread = new QThread(this);
    m_worker = new SshWorker();
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &SshWorker::connected,
            this, &TerminalSessionWidget::onConnected);
    connect(m_worker, &SshWorker::dataReceived,
            this, &TerminalSessionWidget::onDataReceived);
    connect(m_worker, &SshWorker::hostKeyPrompt,
            this, &TerminalSessionWidget::onHostKeyPrompt);
    connect(m_worker, &SshWorker::errorOccurred,
            this, &TerminalSessionWidget::onErrorOccurred);
    connect(m_worker, &SshWorker::disconnected,
            this, &TerminalSessionWidget::onDisconnected);
    connect(m_worker, &SshWorker::directoryListed,
            this, &TerminalSessionWidget::directoryListed);
    connect(m_worker, &SshWorker::pathCanonicalized,
            this, &TerminalSessionWidget::pathCanonicalized);
    connect(m_worker, &SshWorker::sftpFinished,
            this, &TerminalSessionWidget::sftpFinished);
    connect(m_worker, &SshWorker::sftpError,
            this, &TerminalSessionWidget::sftpError);
    connect(m_worker, &SshWorker::sftpCanceled,
            this, &TerminalSessionWidget::sftpCanceled);
    connect(m_worker, &SshWorker::sftpProgress,
            this, &TerminalSessionWidget::sftpProgress);
    connect(m_worker, &SshWorker::sftpUnavailable,
            this, [this](const QString &message) {
                m_sftpAvailable = false;
                m_sftpUnavailableReason = message;
                emit sftpUnavailable(message);
            });
    connect(m_worker, &SshWorker::tunnelStatusChanged,
            this, &TerminalSessionWidget::tunnelStatusChanged);
    connect(m_worker, &SshWorker::tunnelError,
            this, &TerminalSessionWidget::tunnelError);

    m_thread->start();

    int cols = kDefaultPtyCols;
    int rows = kDefaultPtyRows;
    readTerminalSize(&cols, &rows);

    const Connection connection = m_connection;
    const QString secret = m_secret;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, connection, secret, cols, rows]() {
            worker->connectToHost(connection, secret, cols, rows);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::onConnected()
{
    // Assume SFTP works until sftpUnavailable arrives (emitted right after connected).
    m_sftpAvailable = true;
    m_sftpUnavailableReason.clear();

    m_overlay->hide();
    m_term->show();

    if (!m_teletypeStarted) {
        m_term->startTerminalTeletype();
        m_teletypeStarted = true;
        setupPtyBridge();
    } else {
        // Reconnect: keep existing teletype, reattach the PTY write bridge.
        setupPtyBridge();
    }

    m_term->setFocus(Qt::OtherFocusReason);
    m_lastCols = 0;
    m_lastRows = 0;
    // Layout may not have settled yet after show(); sync now and again shortly after.
    schedulePtySizeSync();
    QTimer::singleShot(0, this, &TerminalSessionWidget::syncPtySize);
    QTimer::singleShot(150, this, &TerminalSessionWidget::syncPtySize);
    setState(State::Connected);
    emit statusMessage(tr("Connected: %1").arg(m_displayName));
    startEnabledTunnels();
}

void TerminalSessionWidget::onDataReceived(const QByteArray &data)
{
    if (!m_teletypeStarted || data.isEmpty()) {
        return;
    }

    m_pendingDisplay.append(data);
    if (m_pendingDisplay.size() > kMaxPendingDisplayBytes) {
        // Prefer keeping the newest screen update under pressure (htop redraws).
        m_pendingDisplay = m_pendingDisplay.right(kMaxPendingDisplayBytes / 2);
    }

    flushPendingDisplay();
}

void TerminalSessionWidget::onHostKeyPrompt(SshWorker::HostKeyPrompt reason,
                                            const QString &fingerprint)
{
    bool accept = false;

    if (reason == SshWorker::HostKeyPrompt::Unknown) {
        const auto result = QMessageBox::question(
            this,
            tr("Unknown Host Key"),
            tr("The server host key is not in known_hosts.\n\n"
               "SHA256 fingerprint:\n%1\n\n"
               "Do you trust this host and want to continue?")
                .arg(fingerprint),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        accept = (result == QMessageBox::Yes);
    } else {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Critical);
        if (reason == SshWorker::HostKeyPrompt::Changed) {
            box.setWindowTitle(tr("Host Key Changed"));
            box.setText(tr("WARNING: The host key for this server has changed."));
            box.setInformativeText(
                tr("This may indicate a man-in-the-middle attack, or that the "
                   "server administrator replaced the key.\n\n"
                   "SHA256 fingerprint:\n%1\n\n"
                   "Only continue if you trust the new key.")
                    .arg(fingerprint));
        } else {
            box.setWindowTitle(tr("Host Key Type Mismatch"));
            box.setText(tr("WARNING: The host key type differs from known_hosts."));
            box.setInformativeText(
                tr("An attacker might present a different key type to bypass "
                   "verification.\n\n"
                   "SHA256 fingerprint:\n%1\n\n"
                   "Only continue if you trust this key.")
                    .arg(fingerprint));
        }

        QAbstractButton *abortBtn = box.addButton(tr("Abort"), QMessageBox::RejectRole);
        QAbstractButton *replaceBtn =
            box.addButton(tr("Replace key & continue"), QMessageBox::AcceptRole);
        box.setDefaultButton(qobject_cast<QPushButton *>(abortBtn));
        box.exec();
        accept = (box.clickedButton() == replaceBtn);
    }

    if (m_worker) {
        m_worker->respondHostKeyTrust(accept);
    }
}

void TerminalSessionWidget::onErrorOccurred(const QString &message)
{
    qCWarning(lcSsh) << "Session error:" << message;
    m_sftpAvailable = false;
    m_sftpUnavailableReason.clear();
    teardownPtyBridge();
    showErrorState(message);
    setState(State::Failed);
    emit statusMessage(tr("Error: %1").arg(message));
    emit sessionFailed(message);

    m_shuttingDown = true;
    if (m_worker) {
        disconnect(m_worker, nullptr, this, nullptr);
        QMetaObject::invokeMethod(
            m_worker,
            [worker = m_worker]() {
                worker->disconnectSession();
            },
            Qt::QueuedConnection);
        m_worker = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        m_thread = nullptr;
    }
}

void TerminalSessionWidget::onDisconnected()
{
    m_sftpAvailable = false;
    m_sftpUnavailableReason.clear();
    teardownPtyBridge();

    if (m_shuttingDown) {
        return;
    }

    // Drop / shell exit: keep the tab so the user can reconnect.
    showDisconnectedState();
    setState(State::Disconnected);
    emit statusMessage(tr("Disconnected: %1").arg(m_displayName));
    emit sessionDisconnected();

    // Worker already cleaned SSH; stop the idle thread so reconnect can start fresh.
    m_shuttingDown = true;
    if (m_worker) {
        disconnect(m_worker, nullptr, this, nullptr);
        m_worker = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        m_thread = nullptr;
    }
}

void TerminalSessionWidget::onSendData(const char *data, int length)
{
    if (m_state != State::Connected || m_worker == nullptr || data == nullptr || length <= 0) {
        return;
    }

    const QByteArray bytes(data, length);
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, bytes]() {
            worker->writeToChannel(bytes);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::syncPtySize()
{
    if (!m_teletypeStarted || m_state != State::Connected || m_worker == nullptr || m_term == nullptr) {
        return;
    }

    // Avoid syncing while the widget is momentarily tiny mid-fullscreen transition.
    if (m_term->width() < 20 || m_term->height() < 20) {
        schedulePtySizeSync();
        return;
    }

    int cols = kDefaultPtyCols;
    int rows = kDefaultPtyRows;
    readTerminalSize(&cols, &rows);
    if (cols < kMinTerminalCols || rows < kMinTerminalRows) {
        schedulePtySizeSync();
        return;
    }
    if (cols == m_lastCols && rows == m_lastRows) {
        return;
    }

    m_lastCols = cols;
    m_lastRows = rows;
    syncLocalPtyWinsize(cols, rows);

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, cols, rows]() {
            worker->changePtySize(cols, rows);
        },
        Qt::QueuedConnection);
}

void TerminalSessionWidget::schedulePtySizeSync()
{
    if (m_resizeDebounce) {
        m_resizeDebounce->start();
    }
}

void TerminalSessionWidget::readTerminalSize(int *cols, int *rows) const
{
    int c = m_term ? m_term->screenColumnsCount() : 0;
    int r = m_term ? m_term->screenLinesCount() : 0;

    if ((c < kMinTerminalCols || r < kMinTerminalRows) && m_term) {
        const QWidget *box = (m_term->width() >= 20 && m_term->height() >= 20)
            ? static_cast<const QWidget *>(m_term)
            : static_cast<const QWidget *>(this);
        const QFontMetrics fm(m_term->getTerminalFont());
        const int cw = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
        const int ch = qMax(1, fm.height());
        c = qMax(kMinTerminalCols, box->width() / cw);
        r = qMax(kMinTerminalRows, box->height() / ch);
    }

    if (c < kMinTerminalCols) {
        c = kDefaultPtyCols;
    }
    if (r < kMinTerminalRows) {
        r = kDefaultPtyRows;
    }

    *cols = c;
    *rows = r;
}

void TerminalSessionWidget::syncLocalPtyWinsize(int cols, int rows)
{
    if (!m_term || !m_teletypeStarted) {
        return;
    }
    const int fd = m_term->getPtySlaveFd();
    if (fd < 0) {
        return;
    }

    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ::ioctl(fd, TIOCSWINSZ, &ws);
}

void TerminalSessionWidget::flushPendingDisplay()
{
    if (m_pendingDisplay.isEmpty() || m_term == nullptr || !m_teletypeStarted) {
        if (m_ptyWriteNotifier) {
            m_ptyWriteNotifier->setEnabled(false);
        }
        m_flushTimer->stop();
        return;
    }

    const int fd = m_term->getPtySlaveFd();
    if (fd < 0) {
        return;
    }

    while (!m_pendingDisplay.isEmpty()) {
        const ssize_t written = ::write(
            fd,
            m_pendingDisplay.constData(),
            static_cast<size_t>(m_pendingDisplay.size()));

        if (written > 0) {
            m_pendingDisplay.remove(0, static_cast<int>(written));
            continue;
        }

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            // PTY buffer full: let the emulator drain the master side, then retry.
            if (m_ptyWriteNotifier) {
                m_ptyWriteNotifier->setEnabled(true);
            }
            if (!m_flushTimer->isActive()) {
                m_flushTimer->start();
            }
            return;
        }

        // Hard write error — drop pending to avoid a tight crash loop.
        m_pendingDisplay.clear();
        break;
    }

    if (m_ptyWriteNotifier) {
        m_ptyWriteNotifier->setEnabled(false);
    }
    m_flushTimer->stop();
}

void TerminalSessionWidget::setupPtyBridge()
{
    teardownPtyBridge();

    const int fd = m_term->getPtySlaveFd();
    if (fd < 0) {
        return;
    }

    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    m_ptyWriteNotifier = new QSocketNotifier(fd, QSocketNotifier::Write, this);
    m_ptyWriteNotifier->setEnabled(false);
    connect(m_ptyWriteNotifier, &QSocketNotifier::activated,
            this, &TerminalSessionWidget::flushPendingDisplay);
}

void TerminalSessionWidget::teardownPtyBridge()
{
    m_flushTimer->stop();
    m_pendingDisplay.clear();

    if (m_ptyWriteNotifier) {
        m_ptyWriteNotifier->setEnabled(false);
        delete m_ptyWriteNotifier;
        m_ptyWriteNotifier = nullptr;
    }
}

void TerminalSessionWidget::shutdownWorker()
{
    if (m_shuttingDown && m_thread == nullptr) {
        return;
    }
    m_shuttingDown = true;

    if (m_worker) {
        disconnect(m_worker, nullptr, this, nullptr);
        m_worker->respondHostKeyTrust(false);
        QMetaObject::invokeMethod(
            m_worker,
            [worker = m_worker]() {
                worker->disconnectSession();
            },
            Qt::QueuedConnection);
    }

    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        m_thread = nullptr;
    }

    m_worker = nullptr;
}

void TerminalSessionWidget::clearSecret()
{
    m_secret.fill(QChar(u'\0'));
    m_secret.clear();
}

void TerminalSessionWidget::showConnectingState()
{
    if (!m_teletypeStarted) {
        m_term->hide();
    }
    m_overlay->setText(tr("Connecting…"));
    m_overlay->setEnabled(true);
    m_overlay->show();
}

void TerminalSessionWidget::showDisconnectedState()
{
    if (m_teletypeStarted) {
        m_term->show();
    } else {
        m_term->hide();
    }
    m_overlay->setText(tr("Disconnected — use Session → Reconnect or the tab menu."));
    m_overlay->setEnabled(true);
    m_overlay->show();
}

void TerminalSessionWidget::showErrorState(const QString &message)
{
    if (m_teletypeStarted) {
        m_term->show();
    } else {
        m_term->hide();
    }
    m_overlay->setText(tr("Connection failed:\n%1\n\nUse Reconnect to try again.").arg(message));
    m_overlay->setEnabled(true);
    m_overlay->show();
}
