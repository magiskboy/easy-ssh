/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/tunnel/TunnelBridge.h"

#include <QList>
#include <QObject>
#include <QString>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>

class QIODevice;
class SshIoLoop;

/**
 * Session-wide OpenSSH ForwardAgent bridge (0..1 per SshWorker).
 * Owns ssh_callbacks_struct for the session lifetime; bridges auth-agent
 * channels to the local SSH_AUTH_SOCK. Not used for login (see SshAuth / P5).
 */
class AgentForwardHost final : public QObject
{
    Q_OBJECT

public:
    explicit AgentForwardHost(QObject *parent = nullptr);
    ~AgentForwardHost() override;

    /// True when SSH_AUTH_SOCK is set and the socket path exists.
    static bool isLocalAgentPresent();
    static QString localAgentSocketPath();

    void setIoLoop(SshIoLoop *loop) { m_loop = loop; }

    /// Register session callbacks and send auth-agent-req on @p shellChannel.
    /// Must be called after channel open (+ optional PTY) and before request_shell.
    bool start(ssh_session session, ssh_channel shellChannel, QString *errorOut = nullptr);

    /// Send auth-agent-req on an additional shell channel (after start()).
    bool requestOnChannel(ssh_channel shellChannel, QString *errorOut = nullptr);

    void stop();

private slots:
    void onBridgeSocketReadyRead();
    void onBridgeSocketDisconnected();

private:
    static ssh_channel onAuthAgentOpen(ssh_session session, void *userdata);

    ssh_channel handleAuthAgentOpen(ssh_session session);
    bool openBridge(ssh_channel channel);
    void wireBridgeSocket(QIODevice *socket);
    TunnelBridge *bridgeForSocket(QIODevice *socket);
    void closeBridge(TunnelBridge *bridge);

    ssh_session m_session = nullptr;
    SshIoLoop *m_loop = nullptr;
    struct ssh_callbacks_struct m_callbacks{};
    QList<TunnelBridge *> m_bridges;
    bool m_started = false;
};
