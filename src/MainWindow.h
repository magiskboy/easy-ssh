#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;
class QTabWidget;
class ConnectionListWidget;
class ConnectionModel;
class FileExplorerWidget;
class SecretStore;
class SessionTabWidget;
class TerminalSessionWidget;
class TunnelListWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void setupUi();
    void setupMenus();
    void setStatusText(const QString &text);
    void updateTerminalActionsEnabled();
    void syncFileExplorerToActiveSession();
    void syncTunnelsToActiveSession();
    void wireActiveSessionStateSync(TerminalSessionWidget *session);
    void openSettings();
    void openShortcuts();
    void openAbout();
    void applyAppSettings();
    void rebindShortcuts();
    QAction *registerAction(const QString &actionId, QAction *action);

    ConnectionModel *m_connectionModel = nullptr;
    SecretStore *m_secretStore = nullptr;
    ConnectionListWidget *m_connectionList = nullptr;
    SessionTabWidget *m_sessionTabs = nullptr;
    FileExplorerWidget *m_fileExplorer = nullptr;
    TunnelListWidget *m_tunnelList = nullptr;
    QTabWidget *m_sideTabs = nullptr;
    QLabel *m_statusLabel = nullptr;
    QList<QAction *> m_terminalActions;
    QHash<QString, QAction *> m_shortcutActions;
    TerminalSessionWidget *m_wiredSessionState = nullptr;
};
