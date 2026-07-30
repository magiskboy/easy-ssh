#pragma once

#include "core/connection/Connection.h"
#include "gui/ErrorNotifier.h"

#include <QHash>
#include <QList>
#include <QTabWidget>
#include <QUuid>

class ConnectionModel;
class TerminalSessionWidget;
class WelcomeWidget;

class SessionTabWidget final : public QTabWidget
{
    Q_OBJECT

public:
    explicit SessionTabWidget(QWidget *parent = nullptr);

    void setConnectionModel(ConnectionModel *model);
    void refreshWelcome();

    void openSshSession(const Connection &connection, const SessionCredentials &credentials);
    void disconnectCurrentSession();
    void reconnectCurrentSession();
    void closeCurrentSession();
    void nextSession();
    void previousSession();
    void applySettingsToAllSessions();

    TerminalSessionWidget *activeTerminal() const;
    QList<TerminalSessionWidget *> allTerminals() const;

signals:
    void sessionOpened(const QString &displayName);
    void sessionClosed(const QString &displayName);
    void activeSessionChanged(const QString &displayName);
    void statusMessage(const QString &message, ErrorNotifier::Level level);
    void openConnectionRequested(const QUuid &id);
    void createConnectionRequested();
    void showConnectionsRequested();

private slots:
    void onTabCloseRequested(int index);
    void onCurrentChanged(int index);
    void onTabContextMenu(const QPoint &pos);

private:
    void ensureWelcomeTab();
    void removeWelcomeTabIfPresent();
    bool isWelcomeTab(int index) const;
    WelcomeWidget *welcomeWidget() const;
    QString makeSessionTitle(const Connection &connection);
    void updateTabPresentation(TerminalSessionWidget *session);
    TerminalSessionWidget *terminalAt(int index) const;

    ConnectionModel *m_connectionModel = nullptr;
    int m_welcomeIndex = -1;
    QHash<QUuid, int> m_sessionSerialByConnection;
};
