#pragma once

#include <QUuid>

#include <libssh/libssh.h>

class QTcpSocket;

/**
 * Shared socket↔SSH-channel byte pipe used by Local/Remote tunnel sessions.
 */
struct TunnelBridge
{
    QUuid tunnelId;
    ssh_channel channel = nullptr;
    QTcpSocket *socket = nullptr;
    bool closing = false;
};

namespace TunnelBridgeIo
{
/// Write socket bytes into the SSH channel. Returns false if the bridge should close.
bool writeSocketToChannel(TunnelBridge *bridge, const QByteArray &data);

/// Non-blocking drain channel → socket. Returns true if the bridge should close.
bool pollChannelToSocket(TunnelBridge *bridge);

/// Abort socket, EOF/close/free channel, delete bridge. Caller removes from list first if needed.
void closeBridge(TunnelBridge *bridge, QObject *socketSignalContext);
} // namespace TunnelBridgeIo
