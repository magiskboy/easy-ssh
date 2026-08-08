/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/session/SessionTypes.h"
#include "core/session/WorkspaceState.h"
#include "core/ssh/SshWorker.h"
#include "gui/ErrorNotifier.h"

#include <QHash>
#include <QSet>
#include <QUuid>
#include <QWidget>

class QLabel;
class QPushButton;
class QTermWidget;
class QTimer;
class ExplorerPageWidget;
class Session;
class TerminalDockHost;
class SystemInfoWidget;
class TerminalIoBridge;

class SessionPage final : public QWidget
{
    Q_OBJECT

public:
    explicit SessionPage(Session *session, QWidget *parent = nullptr);
    ~SessionPage() override;

    Session *session() const { return m_session; }

    void applySettings();
    void copySelection();
    void pasteClipboard();
    void toggleSearch();
    void clearScreen();
    void saveLog();
    void saveScreenshot();
    void setLayoutActive(bool active);
    /// Focus/pin terminal in the dock (e.g. Go to Terminal). No-ops while the terminal is closing.
    void activateTerminal(const QUuid &terminalId);
    void toggleProcessExplorer();
    void toggleContainerExplorer();
    void toggleServiceExplorer();
    void toggleSystemInfo();

    void beginWorkspaceRestore(const WorkspaceSessionEntry &entry);
    WorkspaceSessionEntry captureWorkspaceEntry() const;

signals:
    void statusMessage(const QString &message, ErrorNotifier::Level level);
    void editRequested();
    void closeRequested();
    void deleteRequested();
    /// Ask the host to reconnect using the Session's current in-memory credentials.
    void reconnectRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSessionStateChanged(SessionState state);
    void onTerminalsChanged();
    void onActiveTerminalChanged(const QUuid &terminalId);
    void onTerminalData(const QUuid &terminalId, const QByteArray &data);
    void onHostKeyPrompt(const QString &fingerprint,
                         SshWorker::HostKeyPrompt reason,
                         const QString &contextLabel);
    void onSendData(const char *data, int length);
    void syncPtySize();
    void disconnectOrReconnect();
    void onDockTerminalFocused(const QUuid &terminalId);
    void onDockShellCloseRequested(const QUuid &terminalId);
    void onDockTerminalRenameRequested(const QUuid &terminalId);
    void onTermContextMenuRequested(const QPoint &pos);

private:
    struct Pane
    {
        QTermWidget *term = nullptr;
        TerminalIoBridge *bridge = nullptr;
        bool teletypeStarted = false;
        int lastCols = 0;
        int lastRows = 0;
    };

    void ensurePane(const QUuid &terminalId);
    void removePane(const QUuid &terminalId);
    void
    pinTerminalToLayout(const QUuid &terminalId, int dockArea = 0x10, const QUuid &relativeTo = {});
    void pinTerminalWithSmartLayout(const QUuid &terminalId);
    void applySettingsToTerm(QTermWidget *term);
    Pane *activePane();
    QTermWidget *activeTerm();
    void showOverlay(const QString &message, bool showReconnect);
    void schedulePtySizeSync();
    QSize readTerminalSize(QTermWidget *term) const;
    QString terminalTitle(const QUuid &terminalId) const;
    void continueWorkspaceRestore();
    void openExplorerTool(const QString &toolId);
    void openProcessExplorer();
    void closeProcessExplorer();
    void openContainerExplorer();
    void closeContainerExplorer();
    void openServiceExplorer();
    void closeServiceExplorer();
    void openSystemInfo();
    void closeSystemInfo();

    Session *m_session = nullptr;
    TerminalDockHost *m_dockHost = nullptr;
    ExplorerPageWidget *m_processPage = nullptr;
    ExplorerPageWidget *m_containerPage = nullptr;
    ExplorerPageWidget *m_servicePage = nullptr;
    SystemInfoWidget *m_systemInfoPage = nullptr;
    QWidget *m_overlay = nullptr;
    QLabel *m_overlayLabel = nullptr;
    QPushButton *m_reconnectButton = nullptr;
    QHash<QUuid, Pane> m_panes;
    QSet<QUuid> m_closingTerminalIds;
    QTimer *m_resizeDebounce = nullptr;
    /// Terminal created in the latest terminalsChanged; may use smart layout when activated.
    QUuid m_pendingSmartPinId;
    bool m_restoringWorkspace = false;
    bool m_workspaceRestoreBusy = false;
    WorkspaceSessionEntry m_restoreEntry;
};
