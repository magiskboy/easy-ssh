// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "DynamicTunnelSession.h"

#include <QHostAddress>
#include <QIODevice>
#include <QTcpServer>
#include <QTcpSocket>

DynamicTunnelSession::DynamicTunnelSession(const TunnelDefinition &def,
                                           ssh_session session,
                                           QObject *parent)
    : ITunnelSession(parent), m_def(def), m_session(session)
{
}

DynamicTunnelSession::~DynamicTunnelSession()
{
    stop(false);
}

QString DynamicTunnelSession::sessionError() const
{
    if (m_session == nullptr) {
        return tr("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : tr("Unknown error");
}

bool DynamicTunnelSession::start()
{
    if (m_session == nullptr) {
        emit errorOccurred(m_def.id, tr("SSH session is not connected"));
        emit statusChanged(m_def.id, QStringLiteral("Error"), tr("SSH session is not connected"));
        return false;
    }

    if (m_def.socksAuth == SocksAuthMode::UsernamePassword && m_def.socksPassword.isEmpty()) {
        const QString message = tr("SOCKS password is not available");
        emit errorOccurred(m_def.id, message);
        emit statusChanged(m_def.id, QStringLiteral("Error"), message);
        return false;
    }

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

    m_server = server;
    connect(server, &QTcpServer::newConnection, this, &DynamicTunnelSession::onNewConnection);
    return true;
}

void DynamicTunnelSession::stop(bool emitOff)
{
    const QList<PendingClient *> pending = m_pending;
    for (PendingClient *client : pending) {
        abortPending(client);
    }
    m_pending.clear();

    const QList<TunnelBridge *> bridges = m_bridges;
    for (TunnelBridge *bridge : bridges) {
        closeBridge(bridge);
    }
    m_bridges.clear();

    if (m_server) {
        disconnect(m_server, nullptr, this, nullptr);
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }

    if (emitOff) {
        emit statusChanged(m_def.id, QStringLiteral("Off"), QString());
    }
}

void DynamicTunnelSession::poll()
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

void DynamicTunnelSession::onNewConnection()
{
    if (m_server == nullptr) {
        return;
    }

    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        socket->setParent(this);

        auto *pending = new PendingClient;
        pending->socket = socket;
        pending->handshake =
            new Socks5Handshake(m_def.socksAuth,
                                Socks5Handshake::Credentials{.username = m_def.socksUsername,
                                                             .password = m_def.socksPassword});
        m_pending.append(pending);

        connect(socket, &QTcpSocket::readyRead, this, &DynamicTunnelSession::onHandshakeReadyRead);
        connect(socket,
                &QTcpSocket::disconnected,
                this,
                &DynamicTunnelSession::onHandshakeDisconnected);
    }
}

void DynamicTunnelSession::onHandshakeReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    PendingClient *pending = pendingForSocket(socket);
    if (pending == nullptr || pending->handshake == nullptr) {
        return;
    }

    pending->handshake->process(socket);
    if (!pending->handshake->isDone()) {
        return;
    }

    if (!pending->handshake->succeeded()) {
        abortPending(pending);
        return;
    }

    finishHandshake(pending);
}

void DynamicTunnelSession::onHandshakeDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (PendingClient *pending = pendingForSocket(socket)) {
        abortPending(pending);
    }
}

void DynamicTunnelSession::finishHandshake(PendingClient *pending)
{
    if (pending == nullptr || pending->socket == nullptr || pending->handshake == nullptr) {
        return;
    }

    QTcpSocket *socket = pending->socket;
    const QString destHost = pending->handshake->destHost();
    const quint16 destPort = pending->handshake->destPort();

    disconnect(socket, &QTcpSocket::readyRead, this, &DynamicTunnelSession::onHandshakeReadyRead);
    disconnect(
        socket, &QTcpSocket::disconnected, this, &DynamicTunnelSession::onHandshakeDisconnected);

    m_pending.removeAll(pending);
    delete pending->handshake;
    pending->handshake = nullptr;
    delete pending;

    if (!openForwardBridge(socket, destHost, destPort)) {
        Socks5Handshake::writeConnectReply(socket, false);
        socket->disconnectFromHost();
        socket->deleteLater();
    }
}

bool DynamicTunnelSession::openForwardBridge(QTcpSocket *socket,
                                             const QString &destHost,
                                             quint16 destPort)
{
    ssh_channel channel = ssh_channel_new(m_session);
    if (channel == nullptr) {
        emit errorOccurred(m_def.id, tr("Failed to allocate forward channel"));
        return false;
    }

    const QByteArray remoteHost = destHost.toUtf8();
    const QByteArray sourceHost = socket->peerAddress().toString().toUtf8();
    const int sourcePort = static_cast<int>(socket->peerPort());

    ssh_set_blocking(m_session, 1);
    const int rc =
        ssh_channel_open_forward(channel,
                                 remoteHost.constData(),
                                 destPort,
                                 sourceHost.isEmpty() ? "127.0.0.1" : sourceHost.constData(),
                                 sourcePort > 0 ? sourcePort : 0);
    if (rc != SSH_OK) {
        ssh_set_blocking(m_session, 0);
        ssh_channel_free(channel);
        const QString message = tr("Dynamic forward open failed for %1:%2: %3")
                                    .arg(destHost)
                                    .arg(destPort)
                                    .arg(sessionError());
        emit errorOccurred(m_def.id, message);
        return false;
    }

    // Restore non-blocking so SshIoLoop / shell callbacks keep working (Phase 2).
    ssh_set_blocking(m_session, 0);

    Socks5Handshake::writeConnectReply(socket, true);

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = m_def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    m_bridges.append(bridge);

    connect(socket, &QTcpSocket::readyRead, this, &DynamicTunnelSession::onBridgeSocketReadyRead);
    connect(
        socket, &QTcpSocket::disconnected, this, &DynamicTunnelSession::onBridgeSocketDisconnected);
    return true;
}

void DynamicTunnelSession::abortPending(PendingClient *pending)
{
    if (pending == nullptr) {
        return;
    }
    m_pending.removeAll(pending);
    if (pending->socket) {
        disconnect(pending->socket, nullptr, this, nullptr);
        pending->socket->abort();
        pending->socket->deleteLater();
        pending->socket = nullptr;
    }
    delete pending->handshake;
    pending->handshake = nullptr;
    delete pending;
}

DynamicTunnelSession::PendingClient *DynamicTunnelSession::pendingForSocket(QTcpSocket *socket)
{
    if (socket == nullptr) {
        return nullptr;
    }
    for (PendingClient *pending : m_pending) {
        if (pending && pending->socket == socket) {
            return pending;
        }
    }
    return nullptr;
}

void DynamicTunnelSession::onBridgeSocketReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    TunnelBridge *bridge = bridgeForSocket(socket);
    if (bridge == nullptr) {
        return;
    }
    const QByteArray data = socket->readAll();
    if (!TunnelBridgeIo::writeSocketToChannel(bridge, data)) {
        closeBridge(bridge);
    }
}

void DynamicTunnelSession::onBridgeSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (TunnelBridge *bridge = bridgeForSocket(socket)) {
        closeBridge(bridge);
    }
}

TunnelBridge *DynamicTunnelSession::bridgeForSocket(QIODevice *socket)
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

void DynamicTunnelSession::closeBridge(TunnelBridge *bridge)
{
    if (bridge == nullptr) {
        return;
    }
    m_bridges.removeAll(bridge);
    TunnelBridgeIo::closeBridge(bridge, this);
}
