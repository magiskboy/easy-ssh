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
class QLocalServer;
class QTcpServer;

class LocalTunnelSession final : public ITunnelSession
{
    Q_OBJECT

public:
    LocalTunnelSession(const TunnelDefinition &def, ssh_session session, QObject *parent = nullptr);
    ~LocalTunnelSession() override;

    QUuid id() const override { return m_def.id; }
    TunnelType type() const override { return TunnelType::Local; }

    bool start() override;
    void stop(bool emitOff) override;
    void poll() override;

private slots:
    void onNewTcpConnection();
    void onNewLocalConnection();
    void onBridgeSocketReadyRead();
    void onBridgeSocketDisconnected();

private:
    bool startTcpListen();
    bool startUnixListen();
    bool openForwardBridge(QIODevice *socket, const QString &sourceHost, int sourcePort);
    void wireBridgeSocket(QIODevice *socket);
    TunnelBridge *bridgeForSocket(QIODevice *socket);
    void closeBridge(TunnelBridge *bridge);
    QString sessionError() const;

    TunnelDefinition m_def;
    ssh_session m_session = nullptr;
    QTcpServer *m_tcpServer = nullptr;
    QLocalServer *m_localServer = nullptr;
    QList<TunnelBridge *> m_bridges;
};
