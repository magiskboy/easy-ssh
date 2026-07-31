// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TunnelBridge.h"

#include <QAbstractSocket>
#include <QIODevice>
#include <QLocalSocket>

namespace TunnelBridgeIo
{
bool writeSocketToChannel(TunnelBridge *bridge, const QByteArray &data)
{
    if (bridge == nullptr || bridge->closing || bridge->channel == nullptr || data.isEmpty()) {
        return true;
    }

    const char *ptr = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        const int written =
            ssh_channel_write(bridge->channel, ptr, static_cast<uint32_t>(remaining));
        if (written == SSH_AGAIN) {
            break;
        }
        if (written == SSH_ERROR || written < 0) {
            return false;
        }
        if (written == 0) {
            break;
        }
        ptr += written;
        remaining -= written;
    }
    return true;
}

bool pollChannelToSocket(TunnelBridge *bridge)
{
    if (bridge == nullptr || bridge->closing || bridge->channel == nullptr ||
        bridge->socket == nullptr) {
        return false;
    }

    // Drain channel data before acting on EOF — unread buffered bytes would otherwise be lost.
    char buffer[8192];
    bool sawEof = false;
    bool hardError = false;
    while (true) {
        const int nbytes = ssh_channel_read_nonblocking(bridge->channel, buffer, sizeof(buffer), 0);
        if (nbytes == SSH_AGAIN || nbytes == 0) {
            break;
        }
        if (nbytes == SSH_EOF) {
            sawEof = true;
            break;
        }
        if (nbytes < 0) {
            hardError = true;
            break;
        }
        const qint64 written = bridge->socket->write(buffer, nbytes);
        if (written < 0) {
            return true;
        }
    }

    const bool channelDone = hardError || sawEof || !ssh_channel_is_open(bridge->channel) ||
                             ssh_channel_is_eof(bridge->channel);
    if (!channelDone) {
        return false;
    }

    // Push any Qt-buffered response to the OS before the bridge is torn down.
    if (auto *abstract = qobject_cast<QAbstractSocket *>(bridge->socket)) {
        if (abstract->bytesToWrite() > 0) {
            abstract->flush();
        }
    } else if (auto *local = qobject_cast<QLocalSocket *>(bridge->socket)) {
        if (local->bytesToWrite() > 0) {
            local->flush();
        }
    }

    return true;
}

void closeBridge(TunnelBridge *bridge, QObject *socketSignalContext)
{
    if (bridge == nullptr || bridge->closing) {
        return;
    }
    bridge->closing = true;

    if (bridge->socket) {
        if (socketSignalContext) {
            QObject::disconnect(bridge->socket, nullptr, socketSignalContext, nullptr);
        }
        // Graceful close: flush + disconnectFromHost. abort() discards the write buffer and
        // causes curl "Empty reply" when the SSH channel EOFs in the same poll as the response.
        if (auto *abstract = qobject_cast<QAbstractSocket *>(bridge->socket)) {
            if (abstract->bytesToWrite() > 0) {
                abstract->flush();
            }
            if (abstract->state() != QAbstractSocket::UnconnectedState) {
                abstract->disconnectFromHost();
                if (abstract->bytesToWrite() > 0) {
                    abstract->waitForBytesWritten(1000);
                }
                if (abstract->state() != QAbstractSocket::UnconnectedState) {
                    abstract->waitForDisconnected(1000);
                }
            }
        } else if (auto *local = qobject_cast<QLocalSocket *>(bridge->socket)) {
            if (local->bytesToWrite() > 0) {
                local->flush();
            }
            if (local->state() != QLocalSocket::UnconnectedState) {
                local->disconnectFromServer();
                if (local->bytesToWrite() > 0) {
                    local->waitForBytesWritten(1000);
                }
                if (local->state() != QLocalSocket::UnconnectedState) {
                    local->waitForDisconnected(1000);
                }
            }
        } else {
            bridge->socket->close();
        }
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
} // namespace TunnelBridgeIo
