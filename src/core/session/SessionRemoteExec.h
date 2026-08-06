/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/IRemoteExec.h"

#include <QPointer>

class Session;

/// Forwards IRemoteExec to Session::execCommand / Session::commandFinished.
class SessionRemoteExec final : public IRemoteExec
{
    Q_OBJECT

public:
    explicit SessionRemoteExec(Session *session, QObject *parent = nullptr);

    void execCommand(const QString &requestId, const QString &command) override;

private:
    QPointer<Session> m_session;
};
