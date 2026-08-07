/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ErrorNotifier.h"
#include "core/connection/Connection.h"
#include "core/session/SessionTypes.h"
#include "core/session/WorkspaceState.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QUuid>
#include <optional>

class QAction;
class QEvent;
class QLabel;
class QMenu;
class QStackedWidget;
class QTimer;
class QCloseEvent;
class AboutDialog;
class CommandPaletteDialog;
class ConnectionListWidget;
class ConnectionManagerDialog;
class ConnectionModel;
class FileExplorerWidget;
class SecretStore;
class Session;
class SessionManager;
class SessionSideBar;
class SessionTabWidget;
class SettingsDialog;
class TrayController;
class TunnelListWidget;
class WelcomeWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void restoreOrDefaultGeometry();
    QSize defaultStartupSize() const;
    void setupMenus();
    void setupTray();
    void teardownTray();
    void wireTraySession(Session *session);
    void refreshTray();
    void maybeTrayNotify(const QUuid &key, const QString &title, const QString &message);
    void activateSessionFromTray(const QUuid &connectionId);
    void openRecentFromTray(const QUuid &connectionId);
    void requestQuit();
    void restoreFromTray();
    bool canCloseToTray() const;
    bool confirmQuitWithSessions();
    void maybeShowTrayHint();
    void rebuildConnectionsListMenu();
    void setStatusText(const QString &text,
                       ErrorNotifier::Level level = ErrorNotifier::Level::Status);
    void updateTerminalActionsEnabled();
    void updateSessionStatusInfo();
    void syncSidePanelsToActiveSession();
    void openConnectionById(const QUuid &id);
    void readTargetSecretForConnect(const Connection &connection);
    bool promptPasswordForConnect(const Connection &connection, QString *passwordOut);
    void finishConnect(const Connection &connection, const SessionCredentials &credentials);
    void clearPendingConnect();
    void saveWorkspaceState();
    void scheduleWorkspaceSave();
    void beginWorkspaceRestore();
    void advanceWorkspaceRestore();
    std::optional<WorkspaceSessionEntry> takePendingRestoreEntry(const QUuid &connectionId);
    void wireActiveSessionStateSync(Session *session);
    void showWelcomeScreen();
    void showSessionWorkspace();
    void refreshWelcome();
    void updateWorkspaceVisibility();
    void editConnection(const QUuid &id);
    void onConnectionEdited(const QUuid &id,
                            bool connectivityChanged,
                            bool targetSecretUpdated,
                            const QString &targetSecret,
                            bool gatewaySecretUpdated,
                            const QString &gatewaySecret);
    void deleteConnection(const QUuid &id);
    void openSettings(const QString &initialCategoryId = {});
    void openShortcuts();
    void openAbout();
    void openLogFile();
    void openConnectionManager(const QUuid &selectId = {});
    void openCommandPalette();
    void openQuickConnect();
    void openGoToTerminal();
    void ensureCommandPalette();
    void populatePaletteActions();
    void populatePaletteConnections();
    void populatePaletteTerminals();
    void createConnectionFromQuery(const QString &query);
    void connectAfterCreating(const Connection &connection,
                              const QString &password,
                              bool passwordProvided,
                              const QString &gatewayPassword,
                              bool gatewayPasswordProvided,
                              const QString &gatewayPassphrase,
                              bool gatewayPassphraseProvided);
    void focusTerminal(const QUuid &connectionId, const QUuid &terminalId);
    void applyAppSettings();
    void rebindShortcuts();
    QAction *registerAction(const QString &actionId, QAction *action);
    static QString formatSessionTtl(qint64 seconds);

    ConnectionModel *m_connectionModel = nullptr;
    SecretStore *m_secretStore = nullptr;
    SessionManager *m_sessionManager = nullptr;
    ConnectionListWidget *m_connectionList = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    WelcomeWidget *m_welcome = nullptr;
    SessionTabWidget *m_sessionTabs = nullptr;
    SessionSideBar *m_sideBar = nullptr;
    FileExplorerWidget *m_fileExplorer = nullptr;
    TunnelListWidget *m_tunnelList = nullptr;
    QMenu *m_connectionsListMenu = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_sessionInfoLabel = nullptr;
    QTimer *m_sessionInfoTimer = nullptr;
    QList<QAction *> m_terminalActions;
    QHash<QString, QAction *> m_shortcutActions;
    Session *m_wiredSessionState = nullptr;

    QUuid m_pendingConnectId;
    SessionCredentials m_pendingCredentials;
    bool m_pendingNeedTargetSecret = false;

    bool m_restoringWorkspace = false;
    WorkspaceState m_workspaceRestore;
    QList<QUuid> m_restoreQueue;
    QTimer *m_workspaceSaveTimer = nullptr;

    QPointer<ConnectionManagerDialog> m_connectionManager;
    QPointer<SettingsDialog> m_settingsDialog;
    QPointer<AboutDialog> m_aboutDialog;
    QPointer<CommandPaletteDialog> m_commandPalette;

    TrayController *m_tray = nullptr;
    bool m_forceQuit = false;
    bool m_trayTornDown = false;
    QHash<QUuid, qint64> m_trayNotifyTimes;
    QHash<QUuid, SessionState> m_traySessionStates;
};
