// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SessionPage.h"

#include "ShellDockHost.h"
#include "ShellLayoutPlanner.h"
#include "core/session/Session.h"
#include "core/settings/AppSettings.h"
#include "gui/ErrorNotifier.h"
#include "gui/terminal/TerminalIoBridge.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include <qtermwidget.h>

namespace
{
constexpr int kMinCols = 2;
constexpr int kMinRows = 2;
constexpr int kDefaultCols = 80;
constexpr int kDefaultRows = 24;
} // namespace

SessionPage::SessionPage(Session *session, QWidget *parent) : QWidget(parent), m_session(session)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_overlay = new QWidget(this);
    auto *overlayLayout = new QVBoxLayout(m_overlay);
    m_overlayLabel = new QLabel(m_overlay);
    m_overlayLabel->setAlignment(Qt::AlignCenter);
    m_overlayLabel->setWordWrap(true);
    m_reconnectButton = new QPushButton(tr("Reconnect"), m_overlay);
    connect(m_reconnectButton, &QPushButton::clicked, this, &SessionPage::disconnectOrReconnect);
    overlayLayout->addStretch(1);
    overlayLayout->addWidget(m_overlayLabel);
    overlayLayout->addWidget(m_reconnectButton, 0, Qt::AlignHCenter);
    overlayLayout->addStretch(1);

    m_dockHost = new ShellDockHost(this);
    root->addWidget(m_overlay, 0);
    root->addWidget(m_dockHost, 1);

    connect(m_dockHost, &ShellDockHost::shellFocused, this, &SessionPage::onDockShellFocused);
    connect(
        m_dockHost, &ShellDockHost::dropShellRequested, this, &SessionPage::onDropShellRequested);

    m_resizeDebounce = new QTimer(this);
    m_resizeDebounce->setSingleShot(true);
    m_resizeDebounce->setInterval(80);
    connect(m_resizeDebounce, &QTimer::timeout, this, &SessionPage::syncPtySize);

    connect(m_session, &Session::stateChanged, this, &SessionPage::onSessionStateChanged);
    connect(m_session, &Session::shellsChanged, this, &SessionPage::onShellsChanged);
    connect(m_session, &Session::activeShellChanged, this, &SessionPage::onActiveShellChanged);
    connect(m_session, &Session::shellData, this, &SessionPage::onShellData);
    connect(m_session,
            &Session::hostKeyPrompt,
            this,
            [this](SshWorker::HostKeyPrompt reason,
                   const QString &fingerprint,
                   const QString &contextLabel) {
                onHostKeyPrompt(fingerprint, reason, contextLabel);
            });
    connect(m_session, &Session::statusMessage, this, [this](const QString &msg, int level) {
        emit statusMessage(msg, static_cast<ErrorNotifier::Level>(level));
    });

    onShellsChanged();
    onSessionStateChanged(m_session->state());
}

SessionPage::~SessionPage()
{
    if (m_dockHost) {
        m_dockHost->clearLayout();
    }
}

void SessionPage::setLayoutActive(bool active)
{
    if (m_dockHost) {
        m_dockHost->setLayoutActive(active);
    }
}

void SessionPage::onSessionStateChanged(SessionState state)
{
    switch (state) {
    case SessionState::Connecting:
        showOverlay(tr("Connecting…"), false);
        break;
    case SessionState::Connected:
        m_overlay->hide();
        m_dockHost->show();
        // Initial shell is created while Connecting; pin it now so it always
        // appears on the main screen (smart layout on or off).
        onActiveShellChanged(m_session->activeShellId());
        break;
    case SessionState::Disconnected:
        if (m_dockHost) {
            m_dockHost->clearLayout();
        }
        showOverlay(tr("Disconnected from %1.").arg(m_session->displayName()), true);
        break;
    case SessionState::Failed:
        if (m_dockHost) {
            m_dockHost->clearLayout();
        }
        showOverlay(tr("Connection failed:\n%1").arg(m_session->lastError()), true);
        break;
    }
}

void SessionPage::onShellsChanged()
{
    const QSet<QUuid> previousPaneIds(m_panes.keyBegin(), m_panes.keyEnd());

    const QList<ShellChannelState> shells = m_session->shells();
    QSet<QUuid> alive;
    QUuid newborn;
    for (const ShellChannelState &shell : shells) {
        alive.insert(shell.id);
        if (!previousPaneIds.contains(shell.id)) {
            newborn = shell.id;
        }
        ensurePane(shell.id);
        if (m_dockHost && m_dockHost->isPinned(shell.id)) {
            m_dockHost->setShellTitle(shell.id, shell.title);
        }
    }
    const QList<QUuid> existing = m_panes.keys();
    for (const QUuid &id : existing) {
        if (!alive.contains(id)) {
            removePane(id);
        }
    }

    if (m_session->state() == SessionState::Connected && !newborn.isNull()) {
        m_pendingSmartPinId = newborn;
        if (m_session->activeShellId() != newborn) {
            m_session->setActiveShell(newborn);
        } else {
            onActiveShellChanged(newborn);
        }
    } else {
        onActiveShellChanged(m_session->activeShellId());
    }

    if (m_session->state() == SessionState::Connected && shells.isEmpty()) {
        showOverlay(tr("No shells open.\nClick New Shell in the sidebar or Terminal menu."), false);
        m_reconnectButton->setText(tr("New Shell"));
        m_reconnectButton->setVisible(true);
        disconnect(m_reconnectButton, nullptr, this, nullptr);
        connect(m_reconnectButton, &QPushButton::clicked, this, [this]() {
            if (m_session) {
                m_session->newShell();
            }
        });
    } else if (m_session->state() == SessionState::Connected) {
        disconnect(m_reconnectButton, nullptr, this, nullptr);
        connect(
            m_reconnectButton, &QPushButton::clicked, this, &SessionPage::disconnectOrReconnect);
        m_reconnectButton->setText(tr("Reconnect"));
        m_overlay->hide();
        m_dockHost->show();
    }
}

void SessionPage::onActiveShellChanged(const QUuid &shellId)
{
    if (shellId.isNull() || !m_panes.contains(shellId) || !m_dockHost) {
        return;
    }
    if (m_session->state() != SessionState::Connected) {
        return;
    }
    if (m_dockHost->isPinned(shellId)) {
        m_dockHost->focusShell(shellId);
    } else if (shellId == m_pendingSmartPinId && AppSettings::instance().smartLayout()) {
        pinShellWithSmartLayout(shellId);
    } else {
        pinShellToLayout(shellId);
    }
    if (shellId == m_pendingSmartPinId) {
        m_pendingSmartPinId = {};
    }
    if (Pane *pane = activePane()) {
        pane->term->setFocus(Qt::OtherFocusReason);
        schedulePtySizeSync();
    }
}

void SessionPage::onDockShellFocused(const QUuid &shellId)
{
    if (!m_session || shellId.isNull() || shellId == m_session->activeShellId()) {
        return;
    }
    m_session->setActiveShell(shellId);
}

void SessionPage::onDropShellRequested(const QUuid &shellId, int dockArea)
{
    if (!m_session || shellId.isNull() || !m_panes.contains(shellId)) {
        return;
    }
    m_session->setActiveShell(shellId);
    if (m_dockHost->isPinned(shellId)) {
        m_dockHost->focusShell(shellId);
        return;
    }
    pinShellToLayout(shellId, dockArea);
}

void SessionPage::pinShellToLayout(const QUuid &shellId, int dockArea, const QUuid &relativeTo)
{
    auto it = m_panes.find(shellId);
    if (it == m_panes.end() || !m_dockHost) {
        return;
    }
    Pane &pane = it.value();
    pane.term->show();
    m_dockHost->pinShell(shellId, shellTitle(shellId), pane.term, dockArea, relativeTo);
    schedulePtySizeSync();
}

void SessionPage::pinShellWithSmartLayout(const QUuid &shellId)
{
    ShellLayoutSnapshot snap;
    snap.dockedIds = m_dockHost->dockedShellIds();
    snap.focusedId = m_dockHost->focusedShellId();
    const ShellPlacement placement =
        ShellLayoutPlanner{}.decide(snap, ShellLayoutPlanner::Mode::AlternateFocus);
    pinShellToLayout(shellId, placement.dockArea, placement.relativeTo);
}

QString SessionPage::shellTitle(const QUuid &shellId) const
{
    if (!m_session) {
        return tr("Shell");
    }
    for (const ShellChannelState &shell : m_session->shells()) {
        if (shell.id == shellId) {
            return shell.title;
        }
    }
    return tr("Shell");
}

void SessionPage::ensurePane(const QUuid &shellId)
{
    if (m_panes.contains(shellId)) {
        return;
    }
    Pane pane;
    pane.term = new QTermWidget(0, m_dockHost->termHolder());
    pane.term->hide();
    pane.term->setTerminalSizeHint(false);
    pane.bridge = new TerminalIoBridge(pane.term);
    pane.term->installEventFilter(this);
    connect(
        pane.term, SIGNAL(sendData(const char *, int)), this, SLOT(onSendData(const char *, int)));
    applySettingsToTerm(pane.term);
    m_panes.insert(shellId, pane);

    if (m_session->state() == SessionState::Connected) {
        Pane &p = m_panes[shellId];
        p.term->startTerminalTeletype();
        p.teletypeStarted = true;
        p.bridge->start(p.term);
    }
}

void SessionPage::removePane(const QUuid &shellId)
{
    if (!m_panes.contains(shellId)) {
        return;
    }
    if (m_dockHost) {
        m_dockHost->unpinShell(shellId);
    }
    Pane pane = m_panes.take(shellId);
    if (pane.bridge) {
        pane.bridge->teardown();
    }
    delete pane.term;
}

SessionPage::Pane *SessionPage::activePane()
{
    const QUuid id = m_session ? m_session->activeShellId() : QUuid();
    auto it = m_panes.find(id);
    return it == m_panes.end() ? nullptr : &it.value();
}

QTermWidget *SessionPage::activeTerm()
{
    Pane *p = activePane();
    return p ? p->term : nullptr;
}

void SessionPage::onShellData(const QUuid &shellId, const QByteArray &data)
{
    auto it = m_panes.find(shellId);
    if (it == m_panes.end() || data.isEmpty()) {
        return;
    }
    Pane &pane = it.value();
    if (!pane.teletypeStarted) {
        pane.term->startTerminalTeletype();
        pane.teletypeStarted = true;
        pane.bridge->start(pane.term);
    }
    pane.bridge->feed(data);
}

void SessionPage::onSendData(const char *data, int length)
{
    if (!m_session || data == nullptr || length <= 0) {
        return;
    }
    // Route to the shell that owns the focused term when possible.
    QObject *senderObj = sender();
    for (auto it = m_panes.begin(); it != m_panes.end(); ++it) {
        if (it.value().term == senderObj) {
            m_session->writeToShell(it.key(), QByteArray(data, length));
            return;
        }
    }
    m_session->writeToActiveShell(QByteArray(data, length));
}

void SessionPage::onHostKeyPrompt(const QString &fingerprint,
                                  SshWorker::HostKeyPrompt reason,
                                  const QString &contextLabel)
{
    bool accept = false;
    const QString hostContext =
        contextLabel.isEmpty() ? QString() : contextLabel + QStringLiteral("\n\n");
    if (reason == SshWorker::HostKeyPrompt::Unknown) {
        const auto result = QMessageBox::question(
            this,
            tr("Unknown Host Key"),
            hostContext + tr("SHA256 fingerprint:\n%1\n\nTrust this host?").arg(fingerprint),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        accept = (result == QMessageBox::Yes);
    } else {
        QMessageBox::critical(this,
                              tr("Host Key Changed"),
                              hostContext + tr("Host key changed.\nSHA256:\n%1").arg(fingerprint));
        accept = false;
    }
    m_session->respondHostKeyTrust(accept);
}

void SessionPage::disconnectOrReconnect()
{
    if (!m_session) {
        return;
    }
    if (m_session->state() == SessionState::Connected ||
        m_session->state() == SessionState::Connecting) {
        m_session->disconnectTransport();
    } else {
        QSize sz(kDefaultCols, kDefaultRows);
        if (QTermWidget *term = activeTerm()) {
            sz = readTerminalSize(term);
        }
        m_session->reconnect(sz.width(), sz.height());
    }
}

void SessionPage::applySettingsToTerm(QTermWidget *term)
{
    if (!term) {
        return;
    }
    const auto &settings = AppSettings::instance();
    term->setTerminalFont(settings.terminalFont());
    term->setColorScheme(settings.colorScheme());
    term->setHistorySize(settings.historySize());
    term->setBlinkingCursor(settings.cursorBlink());
    term->setConfirmMultilinePaste(settings.confirmMultilinePaste());

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
    term->setKeyboardCursorShape(shape);
}

void SessionPage::applySettings()
{
    for (auto it = m_panes.begin(); it != m_panes.end(); ++it) {
        applySettingsToTerm(it.value().term);
    }
    schedulePtySizeSync();
}

void SessionPage::copySelection()
{
    if (QTermWidget *t = activeTerm()) {
        t->copyClipboard();
    }
}
void SessionPage::pasteClipboard()
{
    if (QTermWidget *t = activeTerm()) {
        t->pasteClipboard();
    }
}
void SessionPage::toggleSearch()
{
    if (QTermWidget *t = activeTerm()) {
        t->toggleShowSearchBar();
    }
}
void SessionPage::clearScreen()
{
    if (QTermWidget *t = activeTerm()) {
        t->clear();
    }
}

void SessionPage::saveLog()
{
    QTermWidget *t = activeTerm();
    if (!t) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Log"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        t->saveHistory(&file);
    }
}

void SessionPage::saveScreenshot()
{
    QTermWidget *t = activeTerm();
    if (!t) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Screenshot"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("PNG images (*.png)"));
    if (!path.isEmpty()) {
        t->grab().save(path, "PNG");
    }
}

bool SessionPage::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        for (auto it = m_panes.begin(); it != m_panes.end(); ++it) {
            if (it.value().term == watched) {
                schedulePtySizeSync();
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SessionPage::schedulePtySizeSync()
{
    if (m_resizeDebounce) {
        m_resizeDebounce->start();
    }
}

void SessionPage::syncPtySize()
{
    if (!m_session || m_session->state() != SessionState::Connected || !m_dockHost) {
        return;
    }

    const QList<QUuid> pinned = m_dockHost->pinnedShellIds();
    QList<QUuid> targets = pinned;
    if (targets.isEmpty()) {
        const QUuid active = m_session->activeShellId();
        if (!active.isNull()) {
            targets.append(active);
        }
    }

    for (const QUuid &shellId : targets) {
        auto it = m_panes.find(shellId);
        if (it == m_panes.end()) {
            continue;
        }
        Pane &pane = it.value();
        if (!pane.term || pane.term->width() <= 0 || pane.term->height() <= 0) {
            continue;
        }
        if (!pane.term->isVisible()) {
            continue;
        }
        const QSize sz = readTerminalSize(pane.term);
        if (sz.width() == pane.lastCols && sz.height() == pane.lastRows) {
            continue;
        }
        pane.lastCols = sz.width();
        pane.lastRows = sz.height();
        if (pane.bridge) {
            pane.bridge->syncSize(sz.width(), sz.height());
        }
        m_session->changePtySize(shellId, sz.width(), sz.height());
    }
}

QSize SessionPage::readTerminalSize(QTermWidget *term) const
{
    if (!term) {
        return QSize(kDefaultCols, kDefaultRows);
    }
    int c = term->screenColumnsCount();
    int r = term->screenLinesCount();
    if (c < kMinCols || r < kMinRows) {
        const QFontMetrics fm(term->getTerminalFont());
        const int cw = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
        const int ch = qMax(1, fm.height());
        c = qMax(kMinCols, term->width() / cw);
        r = qMax(kMinRows, term->height() / ch);
    }
    return QSize(qMax(kMinCols, c), qMax(kMinRows, r));
}

void SessionPage::showOverlay(const QString &message, bool showReconnect)
{
    m_overlayLabel->setText(message);
    m_reconnectButton->setVisible(showReconnect);
    m_overlay->show();
}
