#include "LocalTunnelSession.h"

#include <QHostAddress>
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
    connect(server, &QTcpServer::newConnection, this, &LocalTunnelSession::onNewConnection);
    return true;
}

void LocalTunnelSession::stop(bool emitOff)
{
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

void LocalTunnelSession::onNewConnection()
{
    if (m_server == nullptr) {
        return;
    }

    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        if (!openForwardBridge(socket)) {
            socket->abort();
            socket->deleteLater();
        }
    }
}

bool LocalTunnelSession::openForwardBridge(QTcpSocket *socket)
{
    ssh_channel channel = ssh_channel_new(m_session);
    if (channel == nullptr) {
        emit errorOccurred(m_def.id, tr("Failed to allocate forward channel"));
        return false;
    }

    const QByteArray remoteHost = m_def.remoteHost.toUtf8();
    const QByteArray sourceHost = socket->peerAddress().toString().toUtf8();
    const int sourcePort = static_cast<int>(socket->peerPort());

    ssh_set_blocking(m_session, 1);

    const int rc =
        ssh_channel_open_forward(channel,
                                 remoteHost.constData(),
                                 m_def.remotePort,
                                 sourceHost.isEmpty() ? "127.0.0.1" : sourceHost.constData(),
                                 sourcePort > 0 ? sourcePort : 0);

    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        const QString message = tr("Forward open failed: %1").arg(sessionError());
        emit errorOccurred(m_def.id, message);
        return false;
    }

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = m_def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    m_bridges.append(bridge);

    socket->setParent(this);
    connect(socket, &QTcpSocket::readyRead, this, &LocalTunnelSession::onBridgeSocketReadyRead);
    connect(
        socket, &QTcpSocket::disconnected, this, &LocalTunnelSession::onBridgeSocketDisconnected);
    return true;
}

void LocalTunnelSession::onBridgeSocketReadyRead()
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

void LocalTunnelSession::onBridgeSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (TunnelBridge *bridge = bridgeForSocket(socket)) {
        closeBridge(bridge);
    }
}

TunnelBridge *LocalTunnelSession::bridgeForSocket(QTcpSocket *socket)
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
