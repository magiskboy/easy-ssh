// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "Session.h"

#include "core/fs/TransferJobStore.h"
#include "core/settings/AppSettings.h"
#include "core/tunnel/TunnelStore.h"
#include "core/util/Logging.h"

#include <QDateTime>
#include <QMetaObject>
#include <QThread>
#include <QTimer>

namespace
{

constexpr int kStatusLevel = 0;
constexpr int kSuccessLevel = 1;
constexpr int kWarningLevel = 2;
constexpr int kErrorLevel = 3;

void markFileUnavailable(FileChannelState &file)
{
    file.available = false;
    file.backend = FsBackend::None;
    file.state = ChannelState::Closed;
    file.unavailableReason.clear();
}

TunnelRunStatus tunnelStatusFromString(const QString &status)
{
    if (status == QLatin1String("Starting")) {
        return TunnelRunStatus::Starting;
    }
    if (status == QLatin1String("Listening") || status == QLatin1String("Running")) {
        return TunnelRunStatus::Running;
    }
    if (status == QLatin1String("Error")) {
        return TunnelRunStatus::Error;
    }
    return TunnelRunStatus::Off;
}

} // namespace

Session::Session(const Connection &connection,
                 const SessionCredentials &credentials,
                 QObject *parent)
    : QObject(parent), m_connection(connection), m_credentials(credentials)
{
    markFileUnavailable(m_file);
}

Session::~Session()
{
    shutdown();
}

void Session::setConnection(const Connection &connection)
{
    m_connection = connection;
}

void Session::setCredentials(const SessionCredentials &credentials)
{
    m_credentials = credentials;
}

QString Session::displayName() const
{
    if (!m_connection.name.isEmpty()) {
        return m_connection.name;
    }
    return QStringLiteral("%1@%2").arg(m_connection.username, m_connection.host);
}

QList<ShellChannelState> Session::shells() const
{
    return m_shells;
}

QList<TunnelChannelState> Session::tunnels() const
{
    return m_tunnels.values();
}

void Session::connectTransport(int cols, int rows, const QUuid &initialShellId)
{
    if (m_state == SessionState::Connecting) {
        return;
    }
    if (m_state == SessionState::Connected && m_worker != nullptr) {
        return;
    }
    if (m_connection.id.isNull()) {
        return;
    }

    m_shells.clear();
    m_activeShellId = {};
    m_nextShellSerial = 1;
    m_tunnels.clear();
    markFileUnavailable(m_file);
    emit fileChanged();
    emit tunnelsChanged();

    m_reconnectCols = cols;
    m_reconnectRows = rows;
    m_autoResumeAttempted = false;

    setState(SessionState::Connecting);
    emit statusMessage(tr("Connecting to %1…").arg(displayName()), kStatusLevel);

    ensureWorker();

    const QUuid shellId = initialShellId.isNull() ? QUuid::createUuid() : initialShellId;
    ShellChannelState shell;
    shell.id = shellId;
    shell.serial = nextShellSerial();
    shell.title = QStringLiteral("Shell %1").arg(shell.serial);
    shell.state = ChannelState::Opening;
    shell.cols = cols;
    shell.rows = rows;
    shell.createdAt = QDateTime::currentDateTimeUtc();
    m_shells.append(shell);
    m_activeShellId = shellId;
    emit shellsChanged();
    emit activeShellChanged(m_activeShellId);

    const Connection connection = m_connection;
    const SessionCredentials credentials = m_credentials;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, connection, credentials, shellId, cols, rows]() {
            worker->connectToHost(connection, credentials, shellId, cols, rows);
        },
        Qt::QueuedConnection);
}

void Session::disconnectTransport()
{
    if (m_state != SessionState::Connected && m_state != SessionState::Connecting) {
        return;
    }

    m_userDisconnect = true;
    m_autoReconnectAttempted = false;

    if (m_worker == nullptr) {
        for (ShellChannelState &shell : m_shells) {
            shell.state = ChannelState::Closed;
        }
        m_activeShellId = {};
        markFileUnavailable(m_file);
        setState(SessionState::Disconnected);
        emit shellsChanged();
        emit activeShellChanged(m_activeShellId);
        emit fileChanged();
        emit statusMessage(tr("Disconnected: %1").arg(displayName()), kWarningLevel);
        return;
    }

    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->disconnectSession(); }, Qt::QueuedConnection);
}

void Session::reconnect(int cols, int rows)
{
    m_userDisconnect = false;
    m_reconnectCols = cols;
    m_reconnectRows = rows;
    if (m_state == SessionState::Connecting) {
        return;
    }
    if (m_connection.id.isNull()) {
        return;
    }

    if (m_worker != nullptr) {
        disconnectTransport();
        teardownWorker();
    }
    m_shuttingDown = false;
    m_userDisconnect = false;
    connectTransport(cols, rows);
}

void Session::shutdown()
{
    m_shuttingDown = true;
    teardownWorker();

    for (ShellChannelState &shell : m_shells) {
        shell.state = ChannelState::Closed;
    }
    m_activeShellId = {};
    m_tunnels.clear();
    markFileUnavailable(m_file);

    if (m_state != SessionState::Disconnected) {
        setState(SessionState::Disconnected);
    }

    emit shellsChanged();
    emit activeShellChanged(m_activeShellId);
    emit fileChanged();
    emit tunnelsChanged();
}

QUuid Session::newShell(int cols, int rows, const QUuid &shellId)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return {};
    }
    if (m_shells.size() >= SshWorker::kMaxShells) {
        emit statusMessage(tr("Maximum of %1 shells per session").arg(SshWorker::kMaxShells),
                           kErrorLevel);
        return {};
    }

    const QUuid id = shellId.isNull() ? QUuid::createUuid() : shellId;
    if (findShell(id) != nullptr) {
        return id;
    }

    ShellChannelState shell;
    shell.id = id;
    shell.serial = nextShellSerial();
    shell.title = QStringLiteral("Shell %1").arg(shell.serial);
    shell.state = ChannelState::Opening;
    shell.cols = cols;
    shell.rows = rows;
    shell.createdAt = QDateTime::currentDateTimeUtc();
    m_shells.append(shell);
    emit shellsChanged();

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, id, cols, rows]() { worker->openShell(id, cols, rows); },
        Qt::QueuedConnection);
    return id;
}

void Session::closeShell(const QUuid &shellId)
{
    if (m_worker == nullptr || shellId.isNull() || findShell(shellId) == nullptr) {
        return;
    }

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, shellId]() { worker->closeShell(shellId); },
        Qt::QueuedConnection);
}

void Session::setActiveShell(const QUuid &shellId)
{
    if (shellId == m_activeShellId) {
        return;
    }
    if (findShell(shellId) == nullptr) {
        return;
    }

    m_activeShellId = shellId;
    emit activeShellChanged(m_activeShellId);
}

void Session::renameShell(const QUuid &shellId, const QString &title)
{
    ShellChannelState *shell = findShell(shellId);
    if (shell == nullptr) {
        return;
    }

    shell->title = title;
    emit shellsChanged();
}

void Session::writeToActiveShell(const QByteArray &data)
{
    if (m_activeShellId.isNull()) {
        return;
    }
    writeToShell(m_activeShellId, data);
}

void Session::writeToShell(const QUuid &shellId, const QByteArray &data)
{
    if (m_state != SessionState::Connected || m_worker == nullptr || shellId.isNull() ||
        data.isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, shellId, data]() { worker->writeToChannel(shellId, data); },
        Qt::QueuedConnection);
}

void Session::changePtySize(const QUuid &shellId, int cols, int rows)
{
    if (m_state != SessionState::Connected || m_worker == nullptr || shellId.isNull()) {
        return;
    }

    if (ShellChannelState *shell = findShell(shellId)) {
        shell->cols = cols;
        shell->rows = rows;
    }

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, shellId, cols, rows]() { worker->changePtySize(shellId, cols, rows); },
        Qt::QueuedConnection);
}

void Session::listDirectory(const QString &path)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path]() { worker->listDirectory(path); },
        Qt::QueuedConnection);
}

void Session::createDirectory(const QString &path)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path]() { worker->createDirectory(path); },
        Qt::QueuedConnection);
}

void Session::renamePath(const QString &from, const QString &to)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, from, to]() { worker->renamePath(from, to); },
        Qt::QueuedConnection);
}

void Session::removePath(const QString &path, bool recursive)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path, recursive]() { worker->removePath(path, recursive); },
        Qt::QueuedConnection);
}

void Session::uploadFiles(const QStringList &localPaths, const QString &remoteDir)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, localPaths, remoteDir]() {
            worker->uploadFiles(localPaths, remoteDir);
        },
        Qt::QueuedConnection);
}

void Session::uploadFileTo(const QString &localPath, const QString &remotePath)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, localPath, remotePath]() {
            worker->uploadFileTo(localPath, remotePath);
        },
        Qt::QueuedConnection);
}

void Session::downloadPaths(const QStringList &remotePaths, const QString &localDir)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, remotePaths, localDir]() {
            worker->downloadPaths(remotePaths, localDir);
        },
        Qt::QueuedConnection);
}

void Session::canonicalizePath(const QString &path)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, path]() { worker->canonicalizePath(path); },
        Qt::QueuedConnection);
}

void Session::cancelTransfer()
{
    if (m_worker == nullptr) {
        return;
    }
    m_worker->cancelTransfer();
}

void Session::resumeInterruptedTransfer()
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker]() { worker->resumeInterruptedTransfer(); },
        Qt::QueuedConnection);
}

void Session::discardInterruptedTransfer()
{
    if (m_worker == nullptr) {
        m_hasResumableTransfer = false;
        emit transferResumableChanged(false);
        TransferJobStore::removeAllForConnection(m_connection.id);
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker]() { worker->discardInterruptedTransfer(); },
        Qt::QueuedConnection);
}

void Session::startTunnel(const TunnelDefinition &def)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, def]() { worker->startTunnel(def); }, Qt::QueuedConnection);
}

void Session::stopTunnel(const QUuid &tunnelId)
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, tunnelId]() { worker->stopTunnel(tunnelId); },
        Qt::QueuedConnection);
}

void Session::stopAllTunnels()
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->stopAllTunnels(); }, Qt::QueuedConnection);
}

void Session::execCommand(const QString &requestId, const QString &command)
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        emit commandFinished(requestId, -1, {}, {}, tr("SSH session is not connected"));
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, requestId, command]() { worker->execCommand(requestId, command); },
        Qt::QueuedConnection);
}

void Session::startEnabledTunnels()
{
    if (m_state != SessionState::Connected || m_worker == nullptr) {
        return;
    }

    const QList<TunnelDefinition> tunnels = TunnelStore::loadForConnection(m_connection.id);
    for (const TunnelDefinition &tunnel : tunnels) {
        if (!tunnel.enabled) {
            continue;
        }
        // SOCKS password is injected by TunnelListWidget after SecretStore read.
        if (tunnel.type == TunnelType::Dynamic &&
            tunnel.socksAuth == SocksAuthMode::UsernamePassword) {
            continue;
        }
        startTunnel(tunnel);
    }
}

void Session::respondHostKeyTrust(bool accept)
{
    if (m_worker == nullptr) {
        return;
    }
    m_worker->respondHostKeyTrust(accept);
}

void Session::setState(SessionState state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    if (m_state == SessionState::Connected) {
        m_connectedAt = QDateTime::currentDateTimeUtc();
    } else if (m_state != SessionState::Connecting) {
        m_connectedAt = {};
    }
    emit stateChanged(m_state);
}

void Session::ensureWorker()
{
    if (m_thread != nullptr && m_worker != nullptr) {
        return;
    }

    m_shuttingDown = false;
    m_thread = new QThread(this);
    m_worker = new SshWorker();
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    wireWorker();
    m_thread->start();
}

void Session::teardownWorker()
{
    if (m_worker != nullptr) {
        disconnect(m_worker, nullptr, this, nullptr);
        m_worker->respondHostKeyTrust(false);
        QMetaObject::invokeMethod(
            m_worker, [worker = m_worker]() { worker->disconnectSession(); }, Qt::QueuedConnection);
    }

    if (m_thread != nullptr) {
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        m_thread = nullptr;
    }

    m_worker = nullptr;
}

void Session::wireWorker()
{
    connect(m_worker, &SshWorker::connected, this, &Session::onWorkerConnected);
    connect(m_worker, &SshWorker::disconnected, this, &Session::onWorkerDisconnected);
    connect(m_worker, &SshWorker::errorOccurred, this, &Session::onWorkerError);
    connect(m_worker, &SshWorker::dataReceived, this, &Session::shellData);
    connect(m_worker, &SshWorker::shellOpened, this, &Session::onShellOpened);
    connect(m_worker, &SshWorker::shellClosed, this, &Session::onShellClosed);
    connect(m_worker, &SshWorker::shellFailed, this, &Session::onShellFailed);
    connect(m_worker, &SshWorker::shellOpenFailed, this, &Session::onShellOpenFailed);
    connect(m_worker, &SshWorker::hostKeyPrompt, this, &Session::hostKeyPrompt);
    connect(m_worker, &SshWorker::directoryListed, this, &Session::directoryListed);
    connect(m_worker, &SshWorker::pathCanonicalized, this, &Session::pathCanonicalized);
    connect(m_worker, &SshWorker::sftpFinished, this, &Session::sftpFinished);
    connect(m_worker, &SshWorker::sftpError, this, &Session::sftpError);
    connect(m_worker, &SshWorker::sftpCanceled, this, &Session::sftpCanceled);
    connect(m_worker, &SshWorker::sftpInterrupted, this, [this](const TransferJob &job) {
        m_hasResumableTransfer = true;
        emit sftpInterrupted(job);
        emit transferResumableChanged(true);
    });
    connect(m_worker, &SshWorker::sftpProgress, this, &Session::sftpProgress);
    connect(m_worker, &SshWorker::transferResumableChanged, this, [this](bool resumable) {
        m_hasResumableTransfer = resumable;
        emit transferResumableChanged(resumable);
    });
    connect(m_worker, &SshWorker::remoteFsOpened, this, [this](int backend) {
        m_file.available = true;
        m_file.state = ChannelState::Open;
        m_file.unavailableReason.clear();
        m_file.backend = static_cast<FsBackend>(backend);
        emit fileChanged();
        tryAutoResumeTransfer();
    });
    connect(m_worker, &SshWorker::sftpUnavailable, this, [this](const QString &message) {
        m_file.available = false;
        m_file.backend = FsBackend::None;
        m_file.state = ChannelState::Closed;
        m_file.unavailableReason = message;
        emit fileChanged();
        emit sftpUnavailable(message);
    });
    connect(m_worker,
            &SshWorker::tunnelStatusChanged,
            this,
            [this](const QUuid &tunnelId, const QString &status, const QString &detail) {
                updateTunnelStatus(tunnelId, tunnelStatusFromString(status), detail);
                emit tunnelStatusChanged(tunnelId, status, detail);
            });
    connect(m_worker, &SshWorker::tunnelError, this, &Session::tunnelError);
    connect(m_worker, &SshWorker::agentForwardingWarning, this, [this](const QString &message) {
        emit statusMessage(message, kWarningLevel);
    });
    connect(m_worker, &SshWorker::commandFinished, this, &Session::commandFinished);
}

int Session::nextShellSerial()
{
    return m_nextShellSerial++;
}

ShellChannelState *Session::findShell(const QUuid &shellId)
{
    for (ShellChannelState &shell : m_shells) {
        if (shell.id == shellId) {
            return &shell;
        }
    }
    return nullptr;
}

void Session::updateTunnelStatus(const QUuid &tunnelId,
                                 TunnelRunStatus status,
                                 const QString &detail)
{
    TunnelChannelState &state = m_tunnels[tunnelId];
    state.tunnelId = tunnelId;
    state.status = status;
    state.detail = detail;
    emit tunnelsChanged();
}

void Session::onWorkerConnected(const QUuid &initialShellId)
{
    // Optimistic until remoteFsOpened / sftpUnavailable settles (same thread queue).
    m_file.available = true;
    m_file.backend = FsBackend::None;
    m_file.state = ChannelState::Open;
    m_file.unavailableReason.clear();
    emit fileChanged();

    if (ShellChannelState *shell = findShell(initialShellId)) {
        shell->state = ChannelState::Open;
        emit shellsChanged();
    }

    m_autoReconnectAttempted = false;
    setState(SessionState::Connected);
    emit statusMessage(tr("Connected: %1").arg(displayName()), kSuccessLevel);
    startEnabledTunnels();
}

void Session::onWorkerDisconnected()
{
    if (m_shuttingDown) {
        return;
    }

    for (ShellChannelState &shell : m_shells) {
        shell.state = ChannelState::Closed;
    }
    m_activeShellId = {};
    markFileUnavailable(m_file);

    setState(SessionState::Disconnected);
    emit shellsChanged();
    emit activeShellChanged(m_activeShellId);
    emit fileChanged();
    emit statusMessage(tr("Disconnected: %1").arg(displayName()), kWarningLevel);

    const bool shouldAutoReconnect =
        !m_userDisconnect && AppSettings::instance().autoReconnect() && !m_autoReconnectAttempted;

    m_shuttingDown = true;
    if (m_worker != nullptr) {
        disconnect(m_worker, nullptr, this, nullptr);
        m_worker = nullptr;
    }
    if (m_thread != nullptr) {
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        m_thread = nullptr;
    }

    if (shouldAutoReconnect) {
        scheduleAutoReconnect();
    }
    m_userDisconnect = false;
}

void Session::scheduleAutoReconnect()
{
    m_autoReconnectAttempted = true;
    emit statusMessage(tr("Reconnecting to %1…").arg(displayName()), kStatusLevel);
    QTimer::singleShot(1000, this, [this]() {
        if (m_state != SessionState::Disconnected && m_state != SessionState::Failed) {
            return;
        }
        m_shuttingDown = false;
        connectTransport(m_reconnectCols, m_reconnectRows);
    });
}

void Session::tryAutoResumeTransfer()
{
    if (!AppSettings::instance().autoResumeTransferAfterReconnect()) {
        return;
    }
    if (m_autoResumeAttempted) {
        return;
    }
    if (m_file.backend != FsBackend::Sftp) {
        return;
    }
    if (!TransferJobStore::loadLatest(m_connection.id)) {
        return;
    }
    m_autoResumeAttempted = true;
    m_hasResumableTransfer = true;
    emit transferResumableChanged(true);
    emit statusMessage(tr("Resuming interrupted transfer…"), kStatusLevel);
    resumeInterruptedTransfer();
}

void Session::onWorkerError(const QString &message)
{
    qCWarning(lcSsh) << "Session error:" << message;
    m_lastError = message;

    for (ShellChannelState &shell : m_shells) {
        shell.state = ChannelState::Closed;
    }
    m_activeShellId = {};
    markFileUnavailable(m_file);

    setState(SessionState::Failed);
    emit shellsChanged();
    emit activeShellChanged(m_activeShellId);
    emit fileChanged();
    emit statusMessage(tr("Error: %1").arg(message), kErrorLevel);
    emit sessionFailed(message);

    m_shuttingDown = true;
    if (m_worker != nullptr) {
        disconnect(m_worker, nullptr, this, nullptr);
        QMetaObject::invokeMethod(
            m_worker, [worker = m_worker]() { worker->disconnectSession(); }, Qt::QueuedConnection);
        m_worker = nullptr;
    }
    if (m_thread != nullptr) {
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        m_thread = nullptr;
    }
}

void Session::onShellOpened(const QUuid &shellId)
{
    ShellChannelState *shell = findShell(shellId);
    if (shell == nullptr) {
        return;
    }

    if (shell->state != ChannelState::Open) {
        shell->state = ChannelState::Open;
        emit shellsChanged();
    }
}

void Session::onShellClosed(const QUuid &shellId)
{
    const bool wasActive = (shellId == m_activeShellId);
    m_shells.removeIf([&shellId](const ShellChannelState &shell) { return shell.id == shellId; });

    if (wasActive) {
        m_activeShellId = {};
        for (const ShellChannelState &shell : m_shells) {
            if (shell.state == ChannelState::Open) {
                m_activeShellId = shell.id;
                break;
            }
        }
        if (m_activeShellId.isNull() && !m_shells.isEmpty()) {
            m_activeShellId = m_shells.first().id;
        }
    }

    emit shellsChanged();
    if (wasActive) {
        emit activeShellChanged(m_activeShellId);
    }
}

void Session::onShellFailed(const QUuid &shellId, const QString &message)
{
    if (ShellChannelState *shell = findShell(shellId)) {
        shell->state = ChannelState::Failed;
        emit shellsChanged();
    }
    emit statusMessage(message, kErrorLevel);
    onShellClosed(shellId);
}

void Session::onShellOpenFailed(const QUuid &shellId, const QString &message)
{
    const bool wasActive = (shellId == m_activeShellId);
    m_shells.removeIf([&shellId](const ShellChannelState &shell) { return shell.id == shellId; });

    if (wasActive) {
        m_activeShellId = {};
        for (const ShellChannelState &shell : m_shells) {
            if (shell.state == ChannelState::Open) {
                m_activeShellId = shell.id;
                break;
            }
        }
        if (m_activeShellId.isNull() && !m_shells.isEmpty()) {
            m_activeShellId = m_shells.first().id;
        }
    }

    emit shellsChanged();
    if (wasActive) {
        emit activeShellChanged(m_activeShellId);
    }
    emit statusMessage(message, kErrorLevel);
}
