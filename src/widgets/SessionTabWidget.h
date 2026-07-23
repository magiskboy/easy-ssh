#pragma once

#include "Connection.h"

#include <QHash>
#include <QList>
#include <QTabWidget>
#include <QUuid>

class TerminalSessionWidget;

class SessionTabWidget final : public QTabWidget {
    Q_OBJECT

public:
    explicit SessionTabWidget(QWidget *parent = nullptr);

    void openSshSession(const Connection &connection, const QString &secret);
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
    void statusMessage(const QString &message);

private slots:
    void onTabCloseRequested(int index);
    void onCurrentChanged(int index);
    void onTabContextMenu(const QPoint &pos);

private:
    void ensureWelcomeTab();
    void removeWelcomeTabIfPresent();
    bool isWelcomeTab(int index) const;
    QString makeSessionTitle(const Connection &connection);
    void updateTabPresentation(TerminalSessionWidget *session);
    TerminalSessionWidget *terminalAt(int index) const;

    int m_welcomeIndex = -1;
    QHash<QUuid, int> m_sessionSerialByConnection;
};
