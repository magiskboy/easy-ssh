#include "SshTunnelManager.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

SshTunnelManager::SshTunnelManager(QObject *parent) : QObject(parent) {}

void SshTunnelManager::setSession(ssh_session session)
{
    m_session = session;
}

QString SshTunnelManager::sessionError() const
{
    if (m_session == nullptr) {
        return tr("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : tr("Unknown error");
}

void SshTunnelManager::poll()
{
    acceptRemoteForwards();
    pollTunnelBridges();
}

void SshTunnelManager::startTunnel(const TunnelDefinition &def)
{
    if (m_session == nullptr) {
        emit tunnelError(def.id, tr("SSH session is not connected"));
        emit tunnelStatusChanged(
            def.id, QStringLiteral("Error"), tr("SSH session is not connected"));
        return;
    }

    if (def.id.isNull() || def.localPort == 0 || def.remotePort == 0) {
        emit tunnelError(def.id, tr("Invalid tunnel definition"));
        emit tunnelStatusChanged(def.id, QStringLiteral("Error"), tr("Invalid tunnel definition"));
        return;
    }

    if (m_tunnels.contains(def.id)) {
        stopTunnel(def.id);
    }

    emit tunnelStatusChanged(def.id, QStringLiteral("Starting"), QString());

    auto *tunnel = new ActiveTunnel;
    tunnel->def = def;
    m_tunnels.insert(def.id, tunnel);

    bool ok = false;
    if (def.type == TunnelType::Remote) {
        ok = startRemoteTunnel(tunnel);
    } else {
        ok = startLocalTunnel(tunnel);
    }

    if (!ok) {
        m_tunnels.remove(def.id);
        destroyTunnel(tunnel, false);
        return;
    }

    emit tunnelStatusChanged(def.id, QStringLiteral("Listening"), QString());
}

void SshTunnelManager::stopTunnel(const QUuid &tunnelId)
{
    ActiveTunnel *tunnel = m_tunnels.take(tunnelId);
    if (tunnel == nullptr) {
        emit tunnelStatusChanged(tunnelId, QStringLiteral("Off"), QString());
        return;
    }

    destroyTunnel(tunnel, true);
}

void SshTunnelManager::stopAll()
{
    const QList<QUuid> ids = m_tunnels.keys();
    for (const QUuid &id : ids) {
        ActiveTunnel *tunnel = m_tunnels.take(id);
        if (tunnel) {
            destroyTunnel(tunnel, true);
        }
    }
}

bool SshTunnelManager::startLocalTunnel(ActiveTunnel *tunnel)
{
    auto *server = new QTcpServer(this);
    QHostAddress address;
    if (tunnel->def.localHost.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 ||
        tunnel->def.localHost == QLatin1String("127.0.0.1")) {
        address = QHostAddress::LocalHost;
    } else if (tunnel->def.localHost.compare(QLatin1String("::1"), Qt::CaseInsensitive) == 0) {
        address = QHostAddress::LocalHostIPv6;
    } else {
        address = QHostAddress(tunnel->def.localHost);
    }
    if (address.isNull()) {
        const QString message = tr("Invalid local bind address: %1").arg(tunnel->def.localHost);
        emit tunnelError(tunnel->def.id, message);
        emit tunnelStatusChanged(tunnel->def.id, QStringLiteral("Error"), message);
        server->deleteLater();
        return false;
    }

    if (!server->listen(address, tunnel->def.localPort)) {
        const QString message =
            tr("Cannot listen on %1: %2").arg(tunnel->def.localAddress(), server->errorString());
        emit tunnelError(tunnel->def.id, message);
        emit tunnelStatusChanged(tunnel->def.id, QStringLiteral("Error"), message);
        server->deleteLater();
        return false;
    }

    tunnel->server = server;
    connect(server, &QTcpServer::newConnection, this, &SshTunnelManager::onLocalTunnelNewConnection);
    return true;
}

bool SshTunnelManager::startRemoteTunnel(ActiveTunnel *tunnel)
{
    const QByteArray address = tunnel->def.remoteHost.toUtf8();
    int boundPort = 0;
    const int rc = ssh_channel_listen_forward(m_session,
                                              address.isEmpty() ? nullptr : address.constData(),
                                              tunnel->def.remotePort,
                                              &boundPort);

    if (rc != SSH_OK) {
        const QString message = tr("Remote listen failed: %1").arg(sessionError());
        emit tunnelError(tunnel->def.id, message);
        emit tunnelStatusChanged(tunnel->def.id, QStringLiteral("Error"), message);
        return false;
    }

    tunnel->remoteListening = true;
    if (boundPort > 0) {
        tunnel->def.remotePort = static_cast<quint16>(boundPort);
    }
    return true;
}

void SshTunnelManager::onLocalTunnelNewConnection()
{
    auto *server = qobject_cast<QTcpServer *>(sender());
    if (server == nullptr) {
        return;
    }

    ActiveTunnel *tunnel = tunnelForServer(server);
    if (tunnel == nullptr) {
        while (server->hasPendingConnections()) {
            QTcpSocket *socket = server->nextPendingConnection();
            if (socket) {
                socket->abort();
                socket->deleteLater();
            }
        }
        return;
    }

    while (server->hasPendingConnections()) {
        QTcpSocket *socket = server->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        if (!openLocalForwardBridge(tunnel, socket)) {
            socket->abort();
            socket->deleteLater();
        }
    }
}

bool SshTunnelManager::openLocalForwardBridge(ActiveTunnel *tunnel, QTcpSocket *socket)
{
    ssh_channel channel = ssh_channel_new(m_session);
    if (channel == nullptr) {
        emit tunnelError(tunnel->def.id, tr("Failed to allocate forward channel"));
        return false;
    }

    const QByteArray remoteHost = tunnel->def.remoteHost.toUtf8();
    const QByteArray sourceHost = socket->peerAddress().toString().toUtf8();
    const int sourcePort = static_cast<int>(socket->peerPort());

    // open_forward must run in blocking mode. ssh_channel_set_blocking(0) flips the
    // whole session non-blocking and later opens return SSH_AGAIN (treated as failure),
    // which broke Disable → Enable and multi-connection use.
    ssh_set_blocking(m_session, 1);

    const int rc =
        ssh_channel_open_forward(channel,
                                 remoteHost.constData(),
                                 tunnel->def.remotePort,
                                 sourceHost.isEmpty() ? "127.0.0.1" : sourceHost.constData(),
                                 sourcePort > 0 ? sourcePort : 0);

    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        const QString message = tr("Forward open failed: %1").arg(sessionError());
        emit tunnelError(tunnel->def.id, message);
        return false;
    }

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = tunnel->def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    tunnel->bridges.append(bridge);

    socket->setParent(this);
    connect(socket, &QTcpSocket::readyRead, this, &SshTunnelManager::onBridgeSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &SshTunnelManager::onBridgeSocketDisconnected);
    return true;
}

void SshTunnelManager::acceptRemoteForwards()
{
    if (m_session == nullptr) {
        return;
    }

    bool hasRemote = false;
    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel && tunnel->remoteListening) {
            hasRemote = true;
            break;
        }
    }
    if (!hasRemote) {
        return;
    }

    // Accept any pending reverse-forward channels (timeout 0 = non-blocking).
    for (int i = 0; i < 8; ++i) {
        int destinationPort = 0;
        char *originator = nullptr;
        int originatorPort = 0;
        ssh_channel channel = ssh_channel_open_forward_port(
            m_session, 0, &destinationPort, &originator, &originatorPort);
        if (originator) {
            ssh_string_free_char(originator);
            originator = nullptr;
        }
        if (channel == nullptr) {
            break;
        }

        ActiveTunnel *match = nullptr;
        for (ActiveTunnel *tunnel : m_tunnels) {
            if (tunnel && tunnel->remoteListening &&
                tunnel->def.remotePort == static_cast<quint16>(destinationPort)) {
                match = tunnel;
                break;
            }
        }

        // Some servers report destination_port as 0; fall back to single remote tunnel.
        if (match == nullptr) {
            for (ActiveTunnel *tunnel : m_tunnels) {
                if (tunnel && tunnel->remoteListening) {
                    match = tunnel;
                    break;
                }
            }
        }

        if (match == nullptr) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            continue;
        }

        if (!openRemoteForwardBridge(match, channel)) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
        }
    }
}

bool SshTunnelManager::openRemoteForwardBridge(ActiveTunnel *tunnel, ssh_channel channel)
{
    auto *socket = new QTcpSocket(this);
    socket->connectToHost(tunnel->def.localHost, tunnel->def.localPort);
    if (!socket->waitForConnected(5000)) {
        const QString message = tr("Cannot connect to local %1: %2")
                                    .arg(tunnel->def.localAddress(), socket->errorString());
        emit tunnelError(tunnel->def.id, message);
        socket->deleteLater();
        return false;
    }

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = tunnel->def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    tunnel->bridges.append(bridge);

    connect(socket, &QTcpSocket::readyRead, this, &SshTunnelManager::onBridgeSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &SshTunnelManager::onBridgeSocketDisconnected);
    return true;
}

void SshTunnelManager::onBridgeSocketReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr) {
        return;
    }

    TunnelBridge *bridge = bridgeForSocket(socket);
    if (bridge == nullptr || bridge->closing || bridge->channel == nullptr) {
        return;
    }

    const QByteArray data = socket->readAll();
    if (data.isEmpty()) {
        return;
    }

    const char *ptr = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        const int written =
            ssh_channel_write(bridge->channel, ptr, static_cast<uint32_t>(remaining));
        if (written == SSH_AGAIN) {
            // Should be rare while session is blocking; avoid dropping the bridge.
            break;
        }
        if (written == SSH_ERROR || written < 0) {
            closeBridge(bridge);
            return;
        }
        if (written == 0) {
            break;
        }
        ptr += written;
        remaining -= written;
    }
}

void SshTunnelManager::onBridgeSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr) {
        return;
    }

    TunnelBridge *bridge = bridgeForSocket(socket);
    if (bridge) {
        closeBridge(bridge);
    }
}

void SshTunnelManager::pollTunnelBridges()
{
    char buffer[8192];
    QList<TunnelBridge *> toClose;

    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel == nullptr) {
            continue;
        }
        for (TunnelBridge *bridge : tunnel->bridges) {
            if (bridge == nullptr || bridge->closing || bridge->channel == nullptr ||
                bridge->socket == nullptr) {
                continue;
            }

            if (!ssh_channel_is_open(bridge->channel) || ssh_channel_is_eof(bridge->channel)) {
                toClose.append(bridge);
                continue;
            }

            while (true) {
                const int nbytes =
                    ssh_channel_read_nonblocking(bridge->channel, buffer, sizeof(buffer), 0);
                if (nbytes == SSH_EOF || nbytes < 0) {
                    toClose.append(bridge);
                    break;
                }
                if (nbytes == 0) {
                    break;
                }
                const qint64 written = bridge->socket->write(buffer, nbytes);
                if (written < 0) {
                    toClose.append(bridge);
                    break;
                }
            }
        }
    }

    for (TunnelBridge *bridge : toClose) {
        closeBridge(bridge);
    }
}

void SshTunnelManager::closeBridge(TunnelBridge *bridge)
{
    if (bridge == nullptr || bridge->closing) {
        return;
    }
    bridge->closing = true;

    ActiveTunnel *owner = m_tunnels.value(bridge->tunnelId, nullptr);
    if (owner) {
        owner->bridges.removeAll(bridge);
    }

    if (bridge->socket) {
        disconnect(bridge->socket, nullptr, this, nullptr);
        bridge->socket->abort();
        bridge->socket->deleteLater();
        bridge->socket = nullptr;
    }

    if (bridge->channel) {
        if (ssh_channel_is_open(bridge->channel)) {
            ssh_channel_send_eof(bridge->channel);
            ssh_channel_close(bridge->channel);
        }
        ssh_channel_free(bridge->channel);
        bridge->channel = nullptr;
    }

    delete bridge;
}

void SshTunnelManager::destroyTunnel(ActiveTunnel *tunnel, bool emitOff)
{
    if (tunnel == nullptr) {
        return;
    }

    const QUuid id = tunnel->def.id;

    const QList<TunnelBridge *> bridges = tunnel->bridges;
    for (TunnelBridge *bridge : bridges) {
        closeBridge(bridge);
    }
    tunnel->bridges.clear();

    if (tunnel->server) {
        disconnect(tunnel->server, nullptr, this, nullptr);
        tunnel->server->close();
        // Free synchronously so the local port can be rebound on Enable.
        delete tunnel->server;
        tunnel->server = nullptr;
    }

    if (tunnel->remoteListening && m_session) {
        const QByteArray address = tunnel->def.remoteHost.toUtf8();
        ssh_channel_cancel_forward(
            m_session, address.isEmpty() ? nullptr : address.constData(), tunnel->def.remotePort);
        tunnel->remoteListening = false;
    }

    delete tunnel;

    if (emitOff) {
        emit tunnelStatusChanged(id, QStringLiteral("Off"), QString());
    }
}

SshTunnelManager::ActiveTunnel *SshTunnelManager::tunnelForServer(QTcpServer *server)
{
    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel && tunnel->server == server) {
            return tunnel;
        }
    }
    return nullptr;
}

SshTunnelManager::TunnelBridge *SshTunnelManager::bridgeForSocket(QTcpSocket *socket)
{
    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel == nullptr) {
            continue;
        }
        for (TunnelBridge *bridge : tunnel->bridges) {
            if (bridge && bridge->socket == socket) {
                return bridge;
            }
        }
    }
    return nullptr;
}
