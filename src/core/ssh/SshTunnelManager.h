#pragma once

#include "core/tunnel/Tunnel.h"

#include <QHash>
#include <QObject>
#include <QUuid>

#include <libssh/libssh.h>

class QTcpServer;
class QTcpSocket;

/**
 * Manages local/remote SSH port forwards on a borrowed ssh_session.
 * Lives on the SshWorker thread as a child QObject. Emits status/error signals
 * that SshWorker re-emits to the GUI.
 */
class SshTunnelManager final : public QObject
{
    Q_OBJECT

public:
    explicit SshTunnelManager(QObject *parent = nullptr);

    void setSession(ssh_session session);
    ssh_session session() const { return m_session; }

    void startTunnel(const TunnelDefinition &def);
    void stopTunnel(const QUuid &tunnelId);
    void stopAll();
    void poll();

signals:
    void tunnelStatusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void tunnelError(const QUuid &tunnelId, const QString &message);

private slots:
    void onLocalTunnelNewConnection();
    void onBridgeSocketReadyRead();
    void onBridgeSocketDisconnected();

private:
    struct TunnelBridge
    {
        QUuid tunnelId;
        ssh_channel channel = nullptr;
        QTcpSocket *socket = nullptr;
        bool closing = false;
    };

    struct ActiveTunnel
    {
        TunnelDefinition def;
        QTcpServer *server = nullptr;
        bool remoteListening = false;
        QList<TunnelBridge *> bridges;
    };

    bool startLocalTunnel(ActiveTunnel *tunnel);
    bool startRemoteTunnel(ActiveTunnel *tunnel);
    void acceptRemoteForwards();
    bool openLocalForwardBridge(ActiveTunnel *tunnel, QTcpSocket *socket);
    bool openRemoteForwardBridge(ActiveTunnel *tunnel, ssh_channel channel);
    void pollTunnelBridges();
    void closeBridge(TunnelBridge *bridge);
    void destroyTunnel(ActiveTunnel *tunnel, bool emitOff);
    ActiveTunnel *tunnelForServer(QTcpServer *server);
    TunnelBridge *bridgeForSocket(QTcpSocket *socket);
    QString sessionError() const;

    ssh_session m_session = nullptr;
    QHash<QUuid, ActiveTunnel *> m_tunnels;
};
