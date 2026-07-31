/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ITunnelSession.h"
#include "Socks5Handshake.h"
#include "TunnelBridge.h"

#include <QList>

class QTcpServer;
class QTcpSocket;

class DynamicTunnelSession final : public ITunnelSession
{
    Q_OBJECT

public:
    DynamicTunnelSession(const TunnelDefinition &def,
                         ssh_session session,
                         QObject *parent = nullptr);
    ~DynamicTunnelSession() override;

    QUuid id() const override { return m_def.id; }
    TunnelType type() const override { return TunnelType::Dynamic; }

    bool start() override;
    void stop(bool emitOff) override;
    void poll() override;

private slots:
    void onNewConnection();
    void onHandshakeReadyRead();
    void onHandshakeDisconnected();
    void onBridgeSocketReadyRead();
    void onBridgeSocketDisconnected();

private:
    struct PendingClient
    {
        QTcpSocket *socket = nullptr;
        Socks5Handshake *handshake = nullptr;
    };

    bool openForwardBridge(QTcpSocket *socket, const QString &destHost, quint16 destPort);
    void finishHandshake(PendingClient *pending);
    void abortPending(PendingClient *pending);
    PendingClient *pendingForSocket(QTcpSocket *socket);
    TunnelBridge *bridgeForSocket(QIODevice *socket);
    void closeBridge(TunnelBridge *bridge);
    QString sessionError() const;

    TunnelDefinition m_def;
    ssh_session m_session = nullptr;
    QTcpServer *m_server = nullptr;
    QList<TunnelBridge *> m_bridges;
    QList<PendingClient *> m_pending;
};
