/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"
#include "gui/ErrorNotifier.h"

#include <QHash>
#include <QList>
#include <QTabWidget>
#include <QUuid>

class ConnectionModel;
class Session;
class SessionManager;
class SessionPage;
class WelcomeWidget;

class SessionTabWidget final : public QTabWidget
{
    Q_OBJECT

public:
    explicit SessionTabWidget(QWidget *parent = nullptr);

    void setConnectionModel(ConnectionModel *model);
    void setSessionManager(SessionManager *manager);
    void refreshWelcome();

    void openSshSession(const Connection &connection, const SessionCredentials &credentials);
    void disconnectCurrentSession();
    void reconnectCurrentSession();
    void closeCurrentSession();
    void nextSession();
    void previousSession();
    void applySettingsToAllSessions();
    void refreshConnectionPresentation(const QUuid &connectionId);

    SessionPage *activeSessionPage() const;
    Session *activeSession() const;
    QList<SessionPage *> allSessionPages() const;
    bool activateConnection(const QUuid &connectionId);

signals:
    void sessionOpened(const QString &displayName);
    void sessionClosed(const QString &displayName);
    void activeSessionChanged(const QString &displayName);
    void statusMessage(const QString &message, ErrorNotifier::Level level);
    void openConnectionRequested(const QUuid &id);
    void createConnectionRequested();
    void showConnectionsRequested();
    void editConnectionRequested(const QUuid &id);
    void deleteConnectionRequested(const QUuid &id);

private slots:
    void onTabCloseRequested(int index);
    void onCurrentChanged(int index);
    void onTabContextMenu(const QPoint &pos);

private:
    void ensureWelcomeTab();
    void removeWelcomeTabIfPresent();
    bool isWelcomeTab(int index) const;
    WelcomeWidget *welcomeWidget() const;
    SessionPage *pageAt(int index) const;
    void updateTabPresentation(SessionPage *page);
    int indexForConnection(const QUuid &connectionId) const;

    ConnectionModel *m_connectionModel = nullptr;
    SessionManager *m_sessionManager = nullptr;
    int m_welcomeIndex = -1;
    QHash<QUuid, SessionPage *> m_pagesByConnection;
};
