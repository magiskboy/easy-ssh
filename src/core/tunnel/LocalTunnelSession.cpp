// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "LocalTunnelSession.h"

#include <QHostAddress>
#include <QIODevice>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTcpServer>
#include <QTcpSocket>

LocalTunnelSession::LocalTunnelSession(const TunnelDefinition &def,
                                       ssh_session session,
                                       QObject *parent)
    : ITunnelSession(parent), m_def(def), m_session(session)
{
}

LocalTunnelSession::~LocalTunnelSession()
{
    stop(false);
}

QString LocalTunnelSession::sessionError() const
{
    if (m_session == nullptr) {
        return tr("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : tr("Unknown error");
}

bool LocalTunnelSession::start()
{
    if (m_session == nullptr) {
        emit errorOccurred(m_def.id, tr("SSH session is not connected"));
        emit statusChanged(m_def.id, QStringLiteral("Error"), tr("SSH session is not connected"));
        return false;
    }

    if (m_def.localKind == TunnelEndpointKind::UnixSocket) {
        return startUnixListen();
    }
    return startTcpListen();
}

bool LocalTunnelSession::startTcpListen()
{
    auto *server = new QTcpServer(this);
    QHostAddress address;
    if (m_def.localHost.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 ||
        m_def.localHost == QLatin1String("127.0.0.1")) {
        address = QHostAddress::LocalHost;
    } else if (m_def.localHost.compare(QLatin1String("::1"), Qt::CaseInsensitive) == 0) {
        address = QHostAddress::LocalHostIPv6;
    } else {
        address = QHostAddress(m_def.localHost);
    }
    if (address.isNull()) {
        const QString message = tr("Invalid local bind address: %1").arg(m_def.localHost);
        emit errorOccurred(m_def.id, message);
        emit statusChanged(m_def.id, QStringLiteral("Error"), message);
        server->deleteLater();
        return false;
    }

    if (!server->listen(address, m_def.localPort)) {
        const QString message =
            tr("Cannot listen on %1: %2").arg(m_def.localAddress(), server->errorString());
        emit errorOccurred(m_def.id, message);
        emit statusChanged(m_def.id, QStringLiteral("Error"), message);
        server->deleteLater();
        return false;
    }

    m_tcpServer = server;
    connect(server, &QTcpServer::newConnection, this, &LocalTunnelSession::onNewTcpConnection);
    return true;
}

bool LocalTunnelSession::startUnixListen()
{
#ifndef Q_OS_UNIX
    const QString message = tr("Local Unix socket bind is not supported on this platform");
    emit errorOccurred(m_def.id, message);
    emit statusChanged(m_def.id, QStringLiteral("Error"), message);
    return false;
#else
    const QString path = m_def.localSocketPath.trimmed();
    QLocalServer::removeServer(path);

    auto *server = new QLocalServer(this);
    server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server->listen(path)) {
        const QString message =
            tr("Cannot listen on Unix socket %1: %2").arg(path, server->errorString());
        emit errorOccurred(m_def.id, message);
        emit statusChanged(m_def.id, QStringLiteral("Error"), message);
        server->deleteLater();
        return false;
    }

    m_localServer = server;
    connect(server, &QLocalServer::newConnection, this, &LocalTunnelSession::onNewLocalConnection);
    return true;
#endif
}

void LocalTunnelSession::stop(bool emitOff)
{
    const QList<TunnelBridge *> bridges = m_bridges;
    for (TunnelBridge *bridge : bridges) {
        closeBridge(bridge);
    }
    m_bridges.clear();

    if (m_tcpServer) {
        disconnect(m_tcpServer, nullptr, this, nullptr);
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }

    if (m_localServer) {
        disconnect(m_localServer, nullptr, this, nullptr);
        const QString path = m_localServer->fullServerName();
        m_localServer->close();
        delete m_localServer;
        m_localServer = nullptr;
        if (!path.isEmpty()) {
            QLocalServer::removeServer(path);
        }
    }

    if (emitOff) {
        emit statusChanged(m_def.id, QStringLiteral("Off"), QString());
    }
}

void LocalTunnelSession::poll()
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

void LocalTunnelSession::onNewTcpConnection()
{
    if (m_tcpServer == nullptr) {
        return;
    }

    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        const QString sourceHost = socket->peerAddress().toString();
        const int sourcePort = static_cast<int>(socket->peerPort());
        if (!openForwardBridge(socket, sourceHost, sourcePort)) {
            socket->abort();
            socket->deleteLater();
        }
    }
}

void LocalTunnelSession::onNewLocalConnection()
{
    if (m_localServer == nullptr) {
        return;
    }

    while (m_localServer->hasPendingConnections()) {
        QLocalSocket *socket = m_localServer->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        if (!openForwardBridge(socket, QStringLiteral("127.0.0.1"), 0)) {
            socket->abort();
            socket->deleteLater();
        }
    }
}

bool LocalTunnelSession::openForwardBridge(QIODevice *socket,
                                           const QString &sourceHost,
                                           int sourcePort)
{
    ssh_channel channel = ssh_channel_new(m_session);
    if (channel == nullptr) {
        emit errorOccurred(m_def.id, tr("Failed to allocate forward channel"));
        return false;
    }

    const QByteArray sourceHostBytes = sourceHost.toUtf8();
    const char *originHost = sourceHostBytes.isEmpty() ? "127.0.0.1" : sourceHostBytes.constData();
    const int originPort = sourcePort > 0 ? sourcePort : 0;

    ssh_set_blocking(m_session, 1);

    int rc = SSH_ERROR;
    if (m_def.remoteKind == TunnelEndpointKind::UnixSocket) {
        const QByteArray path = m_def.remoteSocketPath.toUtf8();
        rc = ssh_channel_open_forward_unix(channel, path.constData(), originHost, originPort);
        if (rc != SSH_OK) {
            ssh_channel_free(channel);
            const QString message = tr("Unix socket forward open failed (server may not support "
                                       "direct-streamlocal): %1")
                                        .arg(sessionError());
            emit errorOccurred(m_def.id, message);
            return false;
        }
    } else {
        const QByteArray remoteHost = m_def.remoteHost.toUtf8();
        rc = ssh_channel_open_forward(
            channel, remoteHost.constData(), m_def.remotePort, originHost, originPort);
        if (rc != SSH_OK) {
            ssh_channel_free(channel);
            const QString message = tr("Forward open failed: %1").arg(sessionError());
            emit errorOccurred(m_def.id, message);
            return false;
        }
    }

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = m_def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    m_bridges.append(bridge);

    wireBridgeSocket(socket);
    return true;
}

void LocalTunnelSession::wireBridgeSocket(QIODevice *socket)
{
    socket->setParent(this);
    connect(socket, &QIODevice::readyRead, this, &LocalTunnelSession::onBridgeSocketReadyRead);

    if (auto *tcp = qobject_cast<QTcpSocket *>(socket)) {
        connect(
            tcp, &QTcpSocket::disconnected, this, &LocalTunnelSession::onBridgeSocketDisconnected);
    } else if (auto *local = qobject_cast<QLocalSocket *>(socket)) {
        connect(local,
                &QLocalSocket::disconnected,
                this,
                &LocalTunnelSession::onBridgeSocketDisconnected);
    }
}

void LocalTunnelSession::onBridgeSocketReadyRead()
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

void LocalTunnelSession::onBridgeSocketDisconnected()
{
    auto *device = qobject_cast<QIODevice *>(sender());
    if (TunnelBridge *bridge = bridgeForSocket(device)) {
        closeBridge(bridge);
    }
}

TunnelBridge *LocalTunnelSession::bridgeForSocket(QIODevice *socket)
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

void LocalTunnelSession::closeBridge(TunnelBridge *bridge)
{
    if (bridge == nullptr) {
        return;
    }
    m_bridges.removeAll(bridge);
    TunnelBridgeIo::closeBridge(bridge, this);
}
