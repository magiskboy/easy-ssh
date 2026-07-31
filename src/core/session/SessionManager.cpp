// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SessionManager.h"

SessionManager::SessionManager(QObject *parent) : QObject(parent) {}

Session *SessionManager::open(const Connection &connection, const SessionCredentials &credentials)
{
    if (Session *existing = m_sessions.value(connection.id, nullptr)) {
        setActive(connection.id);
        return existing;
    }

    auto *session = new Session(connection, credentials, this);
    m_sessions.insert(connection.id, session);

    emit sessionOpened(connection.id);
    setActive(connection.id);
    return session;
}

Session *SessionManager::get(const QUuid &connectionId) const
{
    return m_sessions.value(connectionId, nullptr);
}

Session *SessionManager::active() const
{
    return m_sessions.value(m_activeConnectionId, nullptr);
}

QList<Session *> SessionManager::all() const
{
    return m_sessions.values();
}

void SessionManager::setActive(const QUuid &connectionId)
{
    if (connectionId == m_activeConnectionId) {
        return;
    }
    if (!connectionId.isNull() && !m_sessions.contains(connectionId)) {
        return;
    }

    m_activeConnectionId = connectionId;
    emit activeSessionChanged(m_activeConnectionId);
}

void SessionManager::close(const QUuid &connectionId)
{
    Session *session = m_sessions.take(connectionId);
    if (session == nullptr) {
        return;
    }

    session->shutdown();

    if (m_activeConnectionId == connectionId) {
        m_activeConnectionId = {};
        emit activeSessionChanged(m_activeConnectionId);
    }

    emit sessionClosed(connectionId);
    delete session;
}
