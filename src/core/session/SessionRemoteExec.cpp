// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SessionRemoteExec.h"

#include "core/session/Session.h"

SessionRemoteExec::SessionRemoteExec(Session *session, QObject *parent)
    : IRemoteExec(parent), m_session(session)
{
    if (m_session) {
        connect(m_session,
                &Session::commandFinished,
                this,
                &IRemoteExec::commandFinished);
    }
}

void SessionRemoteExec::execCommand(const QString &requestId, const QString &command)
{
    if (!m_session) {
        emit commandFinished(requestId, -1, {}, {}, tr("SSH session is not connected"));
        return;
    }
    m_session->execCommand(requestId, command);
}
