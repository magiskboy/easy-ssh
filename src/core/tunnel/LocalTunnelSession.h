/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ITunnelSession.h"
#include "TunnelBridge.h"

#include <QList>

class QTcpServer;
class QTcpSocket;

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
    void onNewConnection();
    void onBridgeSocketReadyRead();
    void onBridgeSocketDisconnected();

private:
    bool openForwardBridge(QTcpSocket *socket);
    TunnelBridge *bridgeForSocket(QTcpSocket *socket);
    void closeBridge(TunnelBridge *bridge);
    QString sessionError() const;

    TunnelDefinition m_def;
    ssh_session m_session = nullptr;
    QTcpServer *m_server = nullptr;
    QList<TunnelBridge *> m_bridges;
};
