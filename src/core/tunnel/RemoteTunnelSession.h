/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ITunnelSession.h"
#include "TunnelBridge.h"

#include <QList>

class QIODevice;
class SshIoLoop;

class RemoteTunnelSession final : public ITunnelSession
{
    Q_OBJECT

public:
    RemoteTunnelSession(const TunnelDefinition &def,
                        ssh_session session,
                        SshIoLoop *loop,
                        QObject *parent = nullptr);
    ~RemoteTunnelSession() override;

    QUuid id() const override { return m_def.id; }
    TunnelType type() const override { return TunnelType::Remote; }
    quint16 remotePort() const { return m_def.remotePort; }
    bool isListening() const { return m_remoteListening; }

    bool start() override;
    void stop(bool emitOff) override;

    /// Attach an already-accepted reverse-forward channel to this tunnel.
    bool attachForwardChannel(ssh_channel channel);

private slots:
    void onBridgeSocketReadyRead();
    void onBridgeSocketDisconnected();

private:
    bool openForwardBridge(ssh_channel channel);
    void wireBridgeSocket(QIODevice *socket);
    TunnelBridge *bridgeForSocket(QIODevice *socket);
    void closeBridge(TunnelBridge *bridge);
    QString sessionError() const;

    TunnelDefinition m_def;
    ssh_session m_session = nullptr;
    SshIoLoop *m_loop = nullptr;
    bool m_remoteListening = false;
    QList<TunnelBridge *> m_bridges;
};

/**
 * Accept pending reverse-forward channels on the session and dispatch to matching
 * RemoteTunnelSession instances (by bound port).
 */
void acceptRemoteForwards(ssh_session session, const QList<RemoteTunnelSession *> &remotes);
