/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/ssh/SshIoHandler.h"

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <functional>
#include <memory>

#include <libssh/libssh.h>

class QIODevice;
class QObject;
class SshIoLoop;

/**
 * Shared socket↔SSH-channel byte pipe used by Local/Remote/Dynamic tunnel sessions
 * and agent forwarding. socket may be QTcpSocket or QLocalSocket (both QIODevice).
 *
 * Channel → socket uses SshIoLoop::registerChannel callbacks.
 * Socket → channel uses Qt readyRead (caller wires that).
 */
struct TunnelBridge
{
    QUuid tunnelId;
    ssh_channel channel = nullptr;
    QIODevice *socket = nullptr;
    /// QObject used to queue close after channel callbacks (session / agent host).
    QObject *owner = nullptr;
    bool closing = false;
    SshIoLoop *loop = nullptr;
    /// Invoked (queued) when the bridge should close after channel EOF/error.
    std::function<void()> requestClose;
    QByteArray pendingToSocket;
    bool closePending = false;
    std::unique_ptr<SshChannelCallbacks> channelSink;
};

namespace TunnelBridgeIo
{
/// Register channel callbacks on @p loop. Sets bridge->loop and bridge->channelSink.
bool attachToLoop(TunnelBridge *bridge, SshIoLoop *loop, QString *error = nullptr);

/// Write socket bytes into the SSH channel. Returns false if the bridge should close.
bool writeSocketToChannel(TunnelBridge *bridge, const QByteArray &data);

/// Abort socket, unregister channel, EOF/close/free channel, delete bridge.
/// Caller removes from list first if needed.
void closeBridge(TunnelBridge *bridge, QObject *socketSignalContext);
} // namespace TunnelBridgeIo
