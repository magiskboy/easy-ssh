/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"
#include "core/session/Session.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QUuid>

class SessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);

    Session *open(const Connection &connection, const SessionCredentials &credentials);
    Session *get(const QUuid &connectionId) const;
    Session *active() const;
    QList<Session *> all() const;
    void setActive(const QUuid &connectionId);
    void close(const QUuid &connectionId);

signals:
    void sessionOpened(const QUuid &connectionId);
    void sessionClosed(const QUuid &connectionId);
    void activeSessionChanged(const QUuid &connectionId);

private:
    QHash<QUuid, Session *> m_sessions;
    QUuid m_activeConnectionId;
};
