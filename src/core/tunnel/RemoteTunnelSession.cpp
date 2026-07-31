// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "RemoteTunnelSession.h"

#include <QIODevice>
#include <QLocalSocket>
#include <QTcpSocket>

RemoteTunnelSession::RemoteTunnelSession(const TunnelDefinition &def,
                                         ssh_session session,
                                         QObject *parent)
    : ITunnelSession(parent), m_def(def), m_session(session)
{
}

RemoteTunnelSession::~RemoteTunnelSession()
{
    stop(false);
}

QString RemoteTunnelSession::sessionError() const
{
    if (m_session == nullptr) {
        return tr("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : tr("Unknown error");
}

bool RemoteTunnelSession::start()
{
    if (m_session == nullptr) {
        emit errorOccurred(m_def.id, tr("SSH session is not connected"));
        emit statusChanged(m_def.id, QStringLiteral("Error"), tr("SSH session is not connected"));
        return false;
    }

    const QByteArray address = m_def.remoteHost.toUtf8();
    int boundPort = 0;
    const int rc = ssh_channel_listen_forward(
        m_session, address.isEmpty() ? nullptr : address.constData(), m_def.remotePort, &boundPort);

    if (rc != SSH_OK) {
        const QString message = tr("Remote listen failed: %1").arg(sessionError());
        emit errorOccurred(m_def.id, message);
        emit statusChanged(m_def.id, QStringLiteral("Error"), message);
        return false;
    }

    m_remoteListening = true;
    if (boundPort > 0) {
        m_def.remotePort = static_cast<quint16>(boundPort);
    }
    return true;
}

void RemoteTunnelSession::stop(bool emitOff)
{
    const QList<TunnelBridge *> bridges = m_bridges;
    for (TunnelBridge *bridge : bridges) {
        closeBridge(bridge);
    }
    m_bridges.clear();

    if (m_remoteListening && m_session) {
        const QByteArray address = m_def.remoteHost.toUtf8();
        ssh_channel_cancel_forward(
            m_session, address.isEmpty() ? nullptr : address.constData(), m_def.remotePort);
        m_remoteListening = false;
    }

    if (emitOff) {
        emit statusChanged(m_def.id, QStringLiteral("Off"), QString());
    }
}

void RemoteTunnelSession::poll()
{
    QList<TunnelBridge *> toClose;
    for (TunnelBridge *bridge : m_bridges) {
        if (TunnelBridgeIo::pollChannelToSocket(bridge)) {
            toClose.append(bridge);
        }
    }
    for (TunnelBridge *bridge : toClose) {
        closeBridge(bridge);
    }
}

bool RemoteTunnelSession::attachForwardChannel(ssh_channel channel)
{
    return openForwardBridge(channel);
}

bool RemoteTunnelSession::openForwardBridge(ssh_channel channel)
{
    QIODevice *socket = nullptr;

    if (m_def.localKind == TunnelEndpointKind::UnixSocket) {
#ifndef Q_OS_UNIX
        const QString message =
            tr("Local Unix socket destination is not supported on this platform");
        emit errorOccurred(m_def.id, message);
        return false;
#else
        auto *local = new QLocalSocket(this);
        local->connectToServer(m_def.localSocketPath);
        if (!local->waitForConnected(5000)) {
            const QString message = tr("Cannot connect to local Unix socket %1: %2")
                                        .arg(m_def.localSocketPath, local->errorString());
            emit errorOccurred(m_def.id, message);
            local->deleteLater();
            return false;
        }
        socket = local;
#endif
    } else {
        auto *tcp = new QTcpSocket(this);
        tcp->connectToHost(m_def.localHost, m_def.localPort);
        if (!tcp->waitForConnected(5000)) {
            const QString message =
                tr("Cannot connect to local %1: %2").arg(m_def.localAddress(), tcp->errorString());
            emit errorOccurred(m_def.id, message);
            tcp->deleteLater();
            return false;
        }
        socket = tcp;
    }

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = m_def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    m_bridges.append(bridge);

    wireBridgeSocket(socket);
    return true;
}

void RemoteTunnelSession::wireBridgeSocket(QIODevice *socket)
{
    connect(socket, &QIODevice::readyRead, this, &RemoteTunnelSession::onBridgeSocketReadyRead);

    if (auto *tcp = qobject_cast<QTcpSocket *>(socket)) {
        connect(
            tcp, &QTcpSocket::disconnected, this, &RemoteTunnelSession::onBridgeSocketDisconnected);
    } else if (auto *local = qobject_cast<QLocalSocket *>(socket)) {
        connect(local,
                &QLocalSocket::disconnected,
                this,
                &RemoteTunnelSession::onBridgeSocketDisconnected);
    }
}

void RemoteTunnelSession::onBridgeSocketReadyRead()
{
    auto *device = qobject_cast<QIODevice *>(sender());
    TunnelBridge *bridge = bridgeForSocket(device);
    if (bridge == nullptr) {
        return;
    }
    const QByteArray data = device->readAll();
    if (!TunnelBridgeIo::writeSocketToChannel(bridge, data)) {
        closeBridge(bridge);
    }
}

void RemoteTunnelSession::onBridgeSocketDisconnected()
{
    auto *device = qobject_cast<QIODevice *>(sender());
    if (TunnelBridge *bridge = bridgeForSocket(device)) {
        closeBridge(bridge);
    }
}

TunnelBridge *RemoteTunnelSession::bridgeForSocket(QIODevice *socket)
{
    if (socket == nullptr) {
        return nullptr;
    }
    for (TunnelBridge *bridge : m_bridges) {
        if (bridge && bridge->socket == socket) {
            return bridge;
        }
    }
    return nullptr;
}

void RemoteTunnelSession::closeBridge(TunnelBridge *bridge)
{
    if (bridge == nullptr) {
        return;
    }
    m_bridges.removeAll(bridge);
    TunnelBridgeIo::closeBridge(bridge, this);
}

void acceptRemoteForwards(ssh_session session, const QList<RemoteTunnelSession *> &remotes)
{
    if (session == nullptr || remotes.isEmpty()) {
        return;
    }

    bool anyListening = false;
    for (RemoteTunnelSession *remote : remotes) {
        if (remote && remote->isListening()) {
            anyListening = true;
            break;
        }
    }
    if (!anyListening) {
        return;
    }

    for (int i = 0; i < 8; ++i) {
        int destinationPort = 0;
        char *originator = nullptr;
        int originatorPort = 0;
        ssh_channel channel = ssh_channel_open_forward_port(
            session, 0, &destinationPort, &originator, &originatorPort);
        if (originator) {
            ssh_string_free_char(originator);
            originator = nullptr;
        }
        if (channel == nullptr) {
            break;
        }

        RemoteTunnelSession *match = nullptr;
        for (RemoteTunnelSession *remote : remotes) {
            if (remote && remote->isListening() &&
                remote->remotePort() == static_cast<quint16>(destinationPort)) {
                match = remote;
                break;
            }
        }

        if (match == nullptr) {
            for (RemoteTunnelSession *remote : remotes) {
                if (remote && remote->isListening()) {
                    match = remote;
                    break;
                }
            }
        }

        if (match == nullptr) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            continue;
        }

        if (!match->attachForwardChannel(channel)) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
        }
    }
}
