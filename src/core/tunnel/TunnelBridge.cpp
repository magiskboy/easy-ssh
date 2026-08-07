// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TunnelBridge.h"

#include "core/ssh/SshIoLoop.h"

#include <QAbstractSocket>
#include <QIODevice>
#include <QLocalSocket>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

namespace
{
class TunnelBridgeChannelSink final : public SshChannelCallbacks
{
public:
    explicit TunnelBridgeChannelSink(TunnelBridge *bridge) : m_bridge(bridge) {}

    int onData(ssh_session session,
               ssh_channel channel,
               void *data,
               uint32_t len,          // NOLINT(bugprone-easily-swappable-parameters)
               int isStderr) override // NOLINT(bugprone-easily-swappable-parameters)
    {
        Q_UNUSED(session);
        Q_UNUSED(channel);
        Q_UNUSED(isStderr);
        if (m_bridge == nullptr || m_bridge->closing || data == nullptr || len == 0) {
            return static_cast<int>(len);
        }

        if (m_bridge->socket == nullptr) {
            scheduleClose();
            return static_cast<int>(len);
        }

        const char *ptr = static_cast<const char *>(data);
        int remaining = static_cast<int>(len);
        while (remaining > 0) {
            const qint64 written = m_bridge->socket->write(ptr, remaining);
            if (written < 0) {
                scheduleClose();
                return static_cast<int>(len);
            }
            if (written == 0) {
                m_bridge->pendingToSocket.append(ptr, remaining);
                break;
            }
            ptr += written;
            remaining -= static_cast<int>(written);
        }
        return static_cast<int>(len);
    }

    void onEof(ssh_session session, ssh_channel channel) override
    {
        Q_UNUSED(session);
        Q_UNUSED(channel);
        flushPending();
        scheduleClose();
    }

    void onClose(ssh_session session, ssh_channel channel) override
    {
        Q_UNUSED(session);
        Q_UNUSED(channel);
        flushPending();
        scheduleClose();
    }

private:
    void flushPending()
    {
        if (m_bridge == nullptr || m_bridge->socket == nullptr ||
            m_bridge->pendingToSocket.isEmpty()) {
            return;
        }
        m_bridge->socket->write(m_bridge->pendingToSocket);
        m_bridge->pendingToSocket.clear();
        if (auto *abstract = qobject_cast<QAbstractSocket *>(m_bridge->socket)) {
            if (abstract->bytesToWrite() > 0) {
                abstract->flush();
            }
        } else if (auto *local = qobject_cast<QLocalSocket *>(m_bridge->socket)) {
            if (local->bytesToWrite() > 0) {
                local->flush();
            }
        }
    }

    void scheduleClose()
    {
        if (m_bridge == nullptr || m_bridge->closePending || m_bridge->closing) {
            return;
        }
        m_bridge->closePending = true;
        if (m_bridge->owner == nullptr || !m_bridge->requestClose) {
            return;
        }
        // Defer: must not free channel/bridge during libssh callback.
        QPointer<QObject> owner = m_bridge->owner;
        auto closeFn = m_bridge->requestClose;
        QMetaObject::invokeMethod(
            m_bridge->owner,
            [owner, closeFn]() {
                if (owner.isNull() || !closeFn) {
                    return;
                }
                closeFn();
            },
            Qt::QueuedConnection);
    }

    TunnelBridge *m_bridge = nullptr;
};
} // namespace

namespace TunnelBridgeIo
{
bool attachToLoop(TunnelBridge *bridge, SshIoLoop *loop, QString *error)
{
    if (bridge == nullptr || loop == nullptr || bridge->channel == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("TunnelBridge: missing loop or channel");
        }
        return false;
    }

    auto sink = std::make_unique<TunnelBridgeChannelSink>(bridge);
    if (!loop->registerChannel(bridge->channel, sink.get(), error)) {
        return false;
    }
    bridge->loop = loop;
    bridge->channelSink = std::move(sink);
    return true;
}

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

void closeBridge(TunnelBridge *bridge, QObject *socketSignalContext)
{
    if (bridge == nullptr || bridge->closing) {
        return;
    }
    bridge->closing = true;

    if (bridge->loop != nullptr && bridge->channel != nullptr) {
        bridge->loop->unregisterChannel(bridge->channel);
    }
    bridge->channelSink.reset();
    bridge->loop = nullptr;
    bridge->requestClose = {};

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
