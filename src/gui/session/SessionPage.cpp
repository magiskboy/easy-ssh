// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SessionPage.h"

#include "TerminalDockHost.h"
#include "TerminalLayoutPlanner.h"
#include "core/session/Session.h"
#include "core/settings/AppSettings.h"
#include "core/util/Logging.h"
#include "gui/ErrorNotifier.h"
#include "gui/explorer/ExplorerPageWidget.h"
#include "gui/explorer/container/ContainerExplorerModule.h"
#include "gui/explorer/process/ProcessExplorerModule.h"
#include "gui/explorer/service/ServiceExplorerModule.h"
#include "gui/explorer/systeminfo/SystemInfoWidget.h"
#include "gui/terminal/TerminalIoBridge.h"
#include "gui/widgets/UiHelpers.h"

#include <QAction>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>
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

    m_dockHost = new TerminalDockHost(this);
    root->addWidget(m_overlay, 0);
    root->addWidget(m_dockHost, 1);

    connect(m_dockHost, &TerminalDockHost::terminalFocused, this, &SessionPage::onDockTerminalFocused);
    connect(m_dockHost,
            &TerminalDockHost::terminalCloseRequested,
            this,
            &SessionPage::onDockShellCloseRequested);
    connect(m_dockHost,
            &TerminalDockHost::terminalRenameRequested,
            this,
            &SessionPage::onDockTerminalRenameRequested);
    connect(m_dockHost, &TerminalDockHost::toolClosed, this, [this](const QString &toolId) {
        if (toolId == QLatin1String("process")) {
            if (m_processPage) {
                m_processPage->unbind();
                m_processPage->deleteLater();
                m_processPage = nullptr;
            }
        } else if (toolId == QLatin1String("container")) {
            if (m_containerPage) {
                m_containerPage->unbind();
                m_containerPage->deleteLater();
                m_containerPage = nullptr;
            }
        } else if (toolId == QLatin1String("service")) {
            if (m_servicePage) {
                m_servicePage->unbind();
                m_servicePage->deleteLater();
                m_servicePage = nullptr;
            }
        } else if (toolId == QLatin1String("systeminfo")) {
            if (m_systemInfoPage) {
                m_systemInfoPage->deleteLater();
                m_systemInfoPage = nullptr;
            }
        }
    });
    connect(m_dockHost,
            &TerminalDockHost::toolContextMenuAboutToShow,
            this,
            [this](const QString &toolId, QMenu *menu) {
                if (toolId == QLatin1String("systeminfo") && m_systemInfoPage && menu) {
                    m_systemInfoPage->appendCopyActions(menu);
                }
            });

    m_resizeDebounce = new QTimer(this);
    m_resizeDebounce->setSingleShot(true);
    m_resizeDebounce->setInterval(80);
    connect(m_resizeDebounce, &QTimer::timeout, this, &SessionPage::syncPtySize);

    connect(m_session, &Session::stateChanged, this, &SessionPage::onSessionStateChanged);
    connect(m_session, &Session::terminalsChanged, this, &SessionPage::onTerminalsChanged);
    connect(m_session, &Session::activeTerminalChanged, this, &SessionPage::onActiveTerminalChanged);
    connect(m_session, &Session::shellData, this, &SessionPage::onTerminalData);
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

    onTerminalsChanged();
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

void SessionPage::beginWorkspaceRestore(const WorkspaceSessionEntry &entry)
{
    m_restoreEntry = entry;
    m_restoringWorkspace =
        !entry.terminals.isEmpty() || !entry.tools.isEmpty() || !entry.dockState.isEmpty();
}

WorkspaceSessionEntry SessionPage::captureWorkspaceEntry() const
{
    WorkspaceSessionEntry entry;
    if (!m_session) {
        return entry;
    }
    entry.connectionId = m_session->connectionId();
    entry.activeTerminalId = m_session->activeTerminalId();
    for (const TerminalChannelState &shell : m_session->terminals()) {
        if (shell.auxiliary) {
            continue;
        }
        WorkspaceTerminalEntry terminalEntry;
        terminalEntry.id = shell.id;
        terminalEntry.title = shell.title;
        entry.terminals.append(terminalEntry);
    }
    if (m_dockHost) {
        entry.tools = m_dockHost->pinnedToolIds();
        entry.activeToolId = m_dockHost->focusedToolId();
        entry.dockState = m_dockHost->saveLayout();
    }
    return entry;
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
        if (m_restoringWorkspace) {
            continueWorkspaceRestore();
        } else {
            // Initial shell is created while Connecting; pin it now so it always
            // appears on the main screen (smart layout on or off).
            onActiveTerminalChanged(m_session->activeTerminalId());
        }
        break;
    case SessionState::Disconnected:
        m_restoringWorkspace = false;
        m_workspaceRestoreBusy = false;
        m_restoreEntry = {};
        m_closingTerminalIds.clear();
        if (m_dockHost) {
            m_dockHost->clearLayout();
        }
        showOverlay(tr("Disconnected from %1.").arg(m_session->displayName()), true);
        break;
    case SessionState::Failed:
        m_restoringWorkspace = false;
        m_workspaceRestoreBusy = false;
        m_restoreEntry = {};
        m_closingTerminalIds.clear();
        if (m_dockHost) {
            m_dockHost->clearLayout();
        }
        showOverlay(tr("Connection failed:\n%1").arg(m_session->lastError()), true);
        break;
    }
}

void SessionPage::onTerminalsChanged()
{
    const QSet<QUuid> previousPaneIds(m_panes.keyBegin(), m_panes.keyEnd());

    const QList<TerminalChannelState> terminals = m_session->terminals();
    QSet<QUuid> alive;
    QUuid newborn;
    for (const TerminalChannelState &shell : terminals) {
        alive.insert(shell.id);
        if (shell.auxiliary) {
            continue;
        }
        if (!previousPaneIds.contains(shell.id)) {
            newborn = shell.id;
        }
        ensurePane(shell.id);
        if (m_dockHost && m_dockHost->isPinned(shell.id)) {
            m_dockHost->setTerminalTitle(shell.id, shell.title);
        }
    }
    bool removedPane = false;
    const QList<QUuid> existing = m_panes.keys();
    for (const QUuid &id : existing) {
        if (!alive.contains(id)) {
            removePane(id);
            removedPane = true;
        }
    }

    if (m_restoringWorkspace) {
        if (m_session->state() == SessionState::Connected) {
            continueWorkspaceRestore();
        }
        return;
    }

    // Only re-focus dock when a visible shell is born or a pane closes.
    // Auxiliary terminals (e.g. service logs dialog) must not steal focus from tool tabs.
    if (m_session->state() == SessionState::Connected && !newborn.isNull()) {
        m_pendingSmartPinId = newborn;
        if (m_session->activeTerminalId() != newborn) {
            m_session->setActiveTerminal(newborn);
        } else {
            onActiveTerminalChanged(newborn);
        }
    } else if (removedPane) {
        onActiveTerminalChanged(m_session->activeTerminalId());
    }

    if (m_session->state() == SessionState::Connected) {
        m_overlay->hide();
        m_dockHost->show();
    }
}

void SessionPage::continueWorkspaceRestore()
{
    if (!m_restoringWorkspace || !m_session || !m_dockHost || m_workspaceRestoreBusy) {
        return;
    }
    if (m_session->state() != SessionState::Connected) {
        return;
    }

    m_workspaceRestoreBusy = true;

    QSet<QUuid> alive;
    for (const TerminalChannelState &shell : m_session->terminals()) {
        alive.insert(shell.id);
    }

    for (const WorkspaceTerminalEntry &spec : m_restoreEntry.terminals) {
        if (alive.contains(spec.id)) {
            continue;
        }
        m_session->newTerminal(kDefaultCols, kDefaultRows, spec.id);
        alive.insert(spec.id);
    }

    for (const WorkspaceTerminalEntry &spec : m_restoreEntry.terminals) {
        if (spec.title.isEmpty()) {
            continue;
        }
        for (const TerminalChannelState &shell : m_session->terminals()) {
            if (shell.id == spec.id && shell.title != spec.title) {
                m_session->renameTerminal(spec.id, spec.title);
                break;
            }
        }
    }

    for (const WorkspaceTerminalEntry &spec : m_restoreEntry.terminals) {
        ensurePane(spec.id);
        if (!m_dockHost->isPinned(spec.id)) {
            pinTerminalToLayout(spec.id);
        }
        if (!spec.title.isEmpty()) {
            m_dockHost->setTerminalTitle(spec.id, spec.title);
        }
    }

    // Recreate explorer docks before ADS restoreState so objectNames match.
    for (const QString &toolId : m_restoreEntry.tools) {
        openExplorerTool(toolId);
    }

    if (m_restoreEntry.terminals.isEmpty() && m_restoreEntry.tools.isEmpty()) {
        onActiveTerminalChanged(m_session->activeTerminalId());
    } else if (!m_restoreEntry.dockState.isEmpty()) {
        if (!m_dockHost->restoreLayout(m_restoreEntry.dockState)) {
            qCWarning(lcGui) << "workspace dock restore failed for" << m_session->connectionId();
        }
    }

    const QString focusTool = m_restoreEntry.activeToolId;
    const QUuid focusTerminal = m_restoreEntry.activeTerminalId.isNull() ? m_session->activeTerminalId()
                                                                   : m_restoreEntry.activeTerminalId;
    if (!focusTool.isEmpty() && m_dockHost->isToolPinned(focusTool)) {
        m_dockHost->focusTool(focusTool);
    } else if (!focusTerminal.isNull()) {
        m_session->setActiveTerminal(focusTerminal);
        m_dockHost->focusTerminal(focusTerminal);
    }

    m_restoringWorkspace = false;
    m_restoreEntry = {};
    m_workspaceRestoreBusy = false;
    m_overlay->hide();
    m_dockHost->show();
}

void SessionPage::activateTerminal(const QUuid &terminalId)
{
    if (!m_session || terminalId.isNull() || m_closingTerminalIds.contains(terminalId)) {
        return;
    }
    if (m_session->activeTerminalId() != terminalId) {
        m_session->setActiveTerminal(terminalId);
    } else {
        // setActiveTerminal no-ops for the same id; still re-pin after clearLayout edge cases.
        onActiveTerminalChanged(terminalId);
    }
}

void SessionPage::onActiveTerminalChanged(const QUuid &terminalId)
{
    if (terminalId.isNull() || !m_panes.contains(terminalId) || !m_dockHost) {
        return;
    }
    if (m_closingTerminalIds.contains(terminalId)) {
        return;
    }
    if (m_session->state() != SessionState::Connected) {
        return;
    }
    if (m_dockHost->isPinned(terminalId)) {
        m_dockHost->focusTerminal(terminalId);
    } else if (terminalId == m_pendingSmartPinId && AppSettings::instance().smartLayout()) {
        pinTerminalWithSmartLayout(terminalId);
    } else {
        pinTerminalToLayout(terminalId);
    }
    if (terminalId == m_pendingSmartPinId) {
        m_pendingSmartPinId = {};
    }
    if (Pane *pane = activePane()) {
        pane->term->setFocus(Qt::OtherFocusReason);
        schedulePtySizeSync();
    }
}

void SessionPage::onDockTerminalFocused(const QUuid &terminalId)
{
    if (!m_session || terminalId.isNull() || terminalId == m_session->activeTerminalId()) {
        return;
    }
    if (m_closingTerminalIds.contains(terminalId)) {
        return;
    }
    m_session->setActiveTerminal(terminalId);
}

void SessionPage::onDockShellCloseRequested(const QUuid &terminalId)
{
    if (!m_session || terminalId.isNull()) {
        return;
    }
    if (m_closingTerminalIds.contains(terminalId)) {
        return;
    }
    m_closingTerminalIds.insert(terminalId);
    m_session->closeTerminal(terminalId);
}

void SessionPage::onDockTerminalRenameRequested(const QUuid &terminalId)
{
    if (!m_session || terminalId.isNull() || m_closingTerminalIds.contains(terminalId)) {
        return;
    }
    const QString current = terminalTitle(terminalId);
    bool ok = false;
    const QString name = UiHelpers::getText(this, {tr("Rename Terminal"), tr("Name:"), current}, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    m_session->renameTerminal(terminalId, name.trimmed());
    if (m_dockHost) {
        m_dockHost->setTerminalTitle(terminalId, name.trimmed());
    }
}

void SessionPage::onTermContextMenuRequested(const QPoint &pos)
{
    auto *term = qobject_cast<QTermWidget *>(sender());
    if (!term) {
        return;
    }

    QUuid terminalId;
    for (auto it = m_panes.constBegin(); it != m_panes.constEnd(); ++it) {
        if (it.value().term == term) {
            terminalId = it.key();
            break;
        }
    }
    if (terminalId.isNull()) {
        return;
    }

    if (m_session) {
        m_session->setActiveTerminal(terminalId);
    }
    if (m_dockHost) {
        m_dockHost->focusTerminal(terminalId);
    }

    QMenu menu(this);
    const QList<QAction *> filterActs = term->filterActions(pos);
    for (QAction *action : filterActs) {
        menu.addAction(action);
    }
    if (!filterActs.isEmpty()) {
        menu.addSeparator();
    }

    QAction *copyAction = menu.addAction(tr("Copy"), this, &SessionPage::copySelection);
    copyAction->setEnabled(!term->selectedText(false).isEmpty());
    menu.addAction(tr("Paste"), this, &SessionPage::pasteClipboard);
    menu.addSeparator();
    menu.addAction(tr("Clear Screen"), this, &SessionPage::clearScreen);
    menu.addAction(tr("Search…"), this, &SessionPage::toggleSearch);
    menu.addSeparator();
    menu.addAction(tr("Save Log…"), this, &SessionPage::saveLog);
    menu.addAction(tr("Save Screenshot…"), this, &SessionPage::saveScreenshot);

    menu.exec(term->mapToGlobal(pos));
}

void SessionPage::pinTerminalToLayout(const QUuid &terminalId, int dockArea, const QUuid &relativeTo)
{
    if (m_closingTerminalIds.contains(terminalId)) {
        return;
    }
    auto it = m_panes.find(terminalId);
    if (it == m_panes.end() || !m_dockHost) {
        return;
    }
    Pane &pane = it.value();
    pane.term->show();
    m_dockHost->pinTerminal(terminalId, terminalTitle(terminalId), pane.term, dockArea, relativeTo);
    schedulePtySizeSync();
}

void SessionPage::pinTerminalWithSmartLayout(const QUuid &terminalId)
{
    TerminalLayoutSnapshot snap;
    snap.dockedIds = m_dockHost->dockedTerminalIds();
    snap.focusedId = m_dockHost->focusedTerminalId();
    const TerminalPlacement placement =
        TerminalLayoutPlanner{}.decide(snap, TerminalLayoutPlanner::Mode::AlternateFocus);
    pinTerminalToLayout(terminalId, placement.dockArea, placement.relativeTo);
}

QString SessionPage::terminalTitle(const QUuid &terminalId) const
{
    if (!m_session) {
        return tr("Terminal");
    }
    for (const TerminalChannelState &shell : m_session->terminals()) {
        if (shell.id == terminalId) {
            return shell.title;
        }
    }
    return tr("Terminal");
}

void SessionPage::ensurePane(const QUuid &terminalId)
{
    if (m_panes.contains(terminalId)) {
        return;
    }
    Pane pane;
    pane.term = new QTermWidget(0, m_dockHost->termHolder());
    pane.term->hide();
    pane.term->setTerminalSizeHint(false);
    pane.term->setContextMenuPolicy(Qt::CustomContextMenu);
    pane.bridge = new TerminalIoBridge(pane.term);
    pane.term->installEventFilter(this);
    connect(pane.term,
            &QWidget::customContextMenuRequested,
            this,
            &SessionPage::onTermContextMenuRequested);
    connect(
        pane.term, SIGNAL(sendData(const char *, int)), this, SLOT(onSendData(const char *, int)));
    applySettingsToTerm(pane.term);
    m_panes.insert(terminalId, pane);

    if (m_session->state() == SessionState::Connected) {
        Pane &p = m_panes[terminalId];
        p.term->startTerminalTeletype();
        p.teletypeStarted = true;
        p.bridge->start(p.term);
    }
}

void SessionPage::removePane(const QUuid &terminalId)
{
    m_closingTerminalIds.remove(terminalId);
    if (!m_panes.contains(terminalId)) {
        return;
    }
    if (m_dockHost) {
        m_dockHost->unpinTerminal(terminalId);
    }
    Pane pane = m_panes.take(terminalId);
    if (pane.bridge) {
        pane.bridge->teardown();
    }
    delete pane.term;
}

SessionPage::Pane *SessionPage::activePane()
{
    const QUuid id = m_session ? m_session->activeTerminalId() : QUuid();
    auto it = m_panes.find(id);
    return it == m_panes.end() ? nullptr : &it.value();
}

QTermWidget *SessionPage::activeTerm()
{
    Pane *p = activePane();
    return p ? p->term : nullptr;
}

void SessionPage::onTerminalData(const QUuid &terminalId, const QByteArray &data)
{
    auto it = m_panes.find(terminalId);
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
            m_session->writeToTerminal(it.key(), QByteArray(data, length));
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
        emit reconnectRequested();
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
    term->setScrollBarPosition(QTermWidget::ScrollBarRight);
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
        // Ask the remote shell to reprint a prompt after the local wipe.
        t->sendText(QStringLiteral("\r"));
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

    const QList<QUuid> pinned = m_dockHost->pinnedTerminalIds();
    QList<QUuid> targets = pinned;
    if (targets.isEmpty()) {
        const QUuid active = m_session->activeTerminalId();
        if (!active.isNull()) {
            targets.append(active);
        }
    }

    for (const QUuid &terminalId : targets) {
        auto it = m_panes.find(terminalId);
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
        m_session->changePtySize(terminalId, sz.width(), sz.height());
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

void SessionPage::toggleProcessExplorer()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_dockHost->isToolPinned(QStringLiteral("process"))) {
        m_dockHost->unpinTool(QStringLiteral("process"));
        return;
    }
    openProcessExplorer();
}

void SessionPage::openExplorerTool(const QString &toolId)
{
    if (toolId == QLatin1String("process")) {
        openProcessExplorer();
    } else if (toolId == QLatin1String("container")) {
        openContainerExplorer();
    } else if (toolId == QLatin1String("service")) {
        openServiceExplorer();
    } else if (toolId == QLatin1String("systeminfo")) {
        openSystemInfo();
    } else if (!toolId.isEmpty()) {
        qCWarning(lcGui) << "workspace restore skipped unknown explorer tool" << toolId;
    }
}

void SessionPage::openProcessExplorer()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_session->state() != SessionState::Connected) {
        emit statusMessage(tr("Connect to a session to open Process Explorer."),
                           ErrorNotifier::Level::Warning);
        return;
    }
    if (m_dockHost->isToolPinned(QStringLiteral("process"))) {
        m_dockHost->focusTool(QStringLiteral("process"));
        return;
    }

    if (!m_processPage) {
        m_processPage = new ExplorerPageWidget(this);
        m_processPage->bind(std::make_unique<ProcessExplorerModule>(), m_session);
    }

    m_dockHost->pinTool(QStringLiteral("process"),
                        tr("Processes"),
                        m_processPage,
                        /* ads::CenterDockWidgetArea */ 0x10);
}

void SessionPage::closeProcessExplorer()
{
    if (m_dockHost && m_dockHost->isToolPinned(QStringLiteral("process"))) {
        m_dockHost->unpinTool(QStringLiteral("process"));
        return;
    }
    if (m_processPage) {
        m_processPage->unbind();
        m_processPage->deleteLater();
        m_processPage = nullptr;
    }
}

void SessionPage::toggleContainerExplorer()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_dockHost->isToolPinned(QStringLiteral("container"))) {
        m_dockHost->unpinTool(QStringLiteral("container"));
        return;
    }
    openContainerExplorer();
}

void SessionPage::openContainerExplorer()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_session->state() != SessionState::Connected) {
        emit statusMessage(tr("Connect to a session to open Container Explorer."),
                           ErrorNotifier::Level::Warning);
        return;
    }
    if (m_dockHost->isToolPinned(QStringLiteral("container"))) {
        m_dockHost->focusTool(QStringLiteral("container"));
        return;
    }

    if (!m_containerPage) {
        m_containerPage = new ExplorerPageWidget(this);
        m_containerPage->bind(std::make_unique<ContainerExplorerModule>(), m_session);
    }

    m_dockHost->pinTool(QStringLiteral("container"),
                        tr("Containers"),
                        m_containerPage,
                        /* ads::CenterDockWidgetArea */ 0x10);
}

void SessionPage::closeContainerExplorer()
{
    if (m_dockHost && m_dockHost->isToolPinned(QStringLiteral("container"))) {
        m_dockHost->unpinTool(QStringLiteral("container"));
        return;
    }
    if (m_containerPage) {
        m_containerPage->unbind();
        m_containerPage->deleteLater();
        m_containerPage = nullptr;
    }
}

void SessionPage::toggleServiceExplorer()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_dockHost->isToolPinned(QStringLiteral("service"))) {
        m_dockHost->unpinTool(QStringLiteral("service"));
        return;
    }
    openServiceExplorer();
}

void SessionPage::openServiceExplorer()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_session->state() != SessionState::Connected) {
        emit statusMessage(tr("Connect to the session before opening Services."),
                           ErrorNotifier::Level::Warning);
        return;
    }

    if (!m_servicePage) {
        m_servicePage = new ExplorerPageWidget(this);
        m_servicePage->bind(std::make_unique<ServiceExplorerModule>(), m_session);
    }

    m_dockHost->pinTool(QStringLiteral("service"),
                        tr("Services"),
                        m_servicePage,
                        /* ads::CenterDockWidgetArea */ 0x10);
}

void SessionPage::closeServiceExplorer()
{
    if (m_dockHost && m_dockHost->isToolPinned(QStringLiteral("service"))) {
        m_dockHost->unpinTool(QStringLiteral("service"));
        return;
    }
    if (m_servicePage) {
        m_servicePage->unbind();
        m_servicePage->deleteLater();
        m_servicePage = nullptr;
    }
}

void SessionPage::toggleSystemInfo()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_dockHost->isToolPinned(QStringLiteral("systeminfo"))) {
        m_dockHost->unpinTool(QStringLiteral("systeminfo"));
        return;
    }
    openSystemInfo();
}

void SessionPage::openSystemInfo()
{
    if (!m_dockHost || !m_session) {
        return;
    }
    if (m_session->state() != SessionState::Connected) {
        emit statusMessage(tr("Connect to a session to open System Info."),
                           ErrorNotifier::Level::Warning);
        return;
    }
    if (m_dockHost->isToolPinned(QStringLiteral("systeminfo"))) {
        m_dockHost->focusTool(QStringLiteral("systeminfo"));
        return;
    }

    if (!m_systemInfoPage) {
        m_systemInfoPage = new SystemInfoWidget(m_session, this);
    }

    m_dockHost->pinTool(QStringLiteral("systeminfo"),
                        tr("System Info"),
                        m_systemInfoPage,
                        /* ads::CenterDockWidgetArea */ 0x10);
}

void SessionPage::closeSystemInfo()
{
    if (m_dockHost && m_dockHost->isToolPinned(QStringLiteral("systeminfo"))) {
        m_dockHost->unpinTool(QStringLiteral("systeminfo"));
        return;
    }
    if (m_systemInfoPage) {
        m_systemInfoPage->deleteLater();
        m_systemInfoPage = nullptr;
    }
}
