/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ErrorNotifier.h"
#include "core/connection/Connection.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QString>
#include <QUuid>

class QAction;
class QLabel;
class QMenu;
class QTimer;
class QCloseEvent;
class ConnectionListWidget;
class ConnectionModel;
class FileExplorerWidget;
class SecretStore;
class Session;
class SessionManager;
class SessionSideBar;
class SessionTabWidget;
class TunnelListWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void restoreOrDefaultGeometry();
    QSize defaultStartupSize() const;
    void setupMenus();
    void rebuildConnectionsListMenu();
    void setStatusText(const QString &text,
                       ErrorNotifier::Level level = ErrorNotifier::Level::Status);
    void updateTerminalActionsEnabled();
    void updateSessionStatusInfo();
    void syncSidePanelsToActiveSession();
    void openConnectionById(const QUuid &id);
    void readTargetSecretForConnect(const Connection &connection);
    void finishConnect(const Connection &connection, const SessionCredentials &credentials);
    void wireActiveSessionStateSync(Session *session);
    void editConnection(const QUuid &id);
    void onConnectionEdited(const QUuid &id,
                            bool connectivityChanged,
                            bool targetSecretUpdated,
                            const QString &targetSecret,
                            bool gatewaySecretUpdated,
                            const QString &gatewaySecret);
    void deleteConnection(const QUuid &id);
    void openSettings();
    void openShortcuts();
    void openAbout();
    void openLogFile();
    void applyAppSettings();
    void rebindShortcuts();
    QAction *registerAction(const QString &actionId, QAction *action);
    static QString formatSessionTtl(qint64 seconds);

    ConnectionModel *m_connectionModel = nullptr;
    SecretStore *m_secretStore = nullptr;
    SessionManager *m_sessionManager = nullptr;
    ConnectionListWidget *m_connectionList = nullptr;
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
};
