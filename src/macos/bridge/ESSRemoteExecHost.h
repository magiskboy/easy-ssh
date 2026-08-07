/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/IRemoteExec.h"
#include "core/ssh/SshWorker.h"

#include <QMetaObject>
#include <QPointer>

/// IRemoteExec adapter over the macOS bridge's SshWorker (no Session object).
class ESSRemoteExecHost final : public IRemoteExec
{
    Q_OBJECT

public:
    explicit ESSRemoteExecHost(QObject *parent = nullptr);

    void setWorker(SshWorker *worker);
    void clearWorker();
    SshWorker *worker() const { return m_worker; }
    bool isConnected() const { return m_connected; }
    void setConnected(bool connected) { m_connected = connected; }

    void execCommand(const QString &requestId, const QString &command) override;

private:
    void wireWorker();
    void unwireWorker();

    QPointer<SshWorker> m_worker;
    QMetaObject::Connection m_finishedConnection;
    bool m_connected = false;
};
