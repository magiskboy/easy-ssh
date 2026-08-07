/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Tunnel.h"

#include <QObject>
#include <QString>
#include <QUuid>

#include <libssh/libssh.h>

class SshIoLoop;

/**
 * Per-active-tunnel interface (parallel to FsEngine).
 * Concrete: LocalTunnelSession, RemoteTunnelSession, DynamicTunnelSession.
 */
class ITunnelSession : public QObject
{
    Q_OBJECT

public:
    explicit ITunnelSession(QObject *parent = nullptr) : QObject(parent) {}
    ~ITunnelSession() override = default;

    virtual QUuid id() const = 0;
    virtual TunnelType type() const = 0;

    /// Bind listen / remote forward. On failure emit error + return false.
    virtual bool start() = 0;
    virtual void stop(bool emitOff) = 0;

signals:
    void statusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void errorOccurred(const QUuid &tunnelId, const QString &message);
};

/// Create Local/Remote/Dynamic session (parented). @p loop registers channel bridges.
ITunnelSession *createTunnelSession(const TunnelDefinition &def,
                                    ssh_session session,
                                    SshIoLoop *loop,
                                    QObject *parent);
