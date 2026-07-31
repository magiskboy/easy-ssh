// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "AgentForwardHost.h"

#include "core/util/Logging.h"

#include <QFileInfo>
#include <QIODevice>
#include <QLocalSocket>

#include <cstring>

AgentForwardHost::AgentForwardHost(QObject *parent) : QObject(parent) {}

AgentForwardHost::~AgentForwardHost()
{
    stop();
}

QString AgentForwardHost::localAgentSocketPath()
{
    return qEnvironmentVariable("SSH_AUTH_SOCK").trimmed();
}

bool AgentForwardHost::isLocalAgentPresent()
{
    const QString path = localAgentSocketPath();
    if (path.isEmpty()) {
        return false;
    }
    return QFileInfo::exists(path);
}

bool AgentForwardHost::start(ssh_session session, ssh_channel firstShellChannel, QString *errorOut)
{
    if (m_started) {
        return true;
    }
    if (session == nullptr || firstShellChannel == nullptr) {
        if (errorOut) {
            *errorOut = tr("SSH session or shell channel is missing");
        }
        return false;
    }

    m_session = session;
    std::memset(&m_callbacks, 0, sizeof(m_callbacks));
    ssh_callbacks_init(&m_callbacks);
    m_callbacks.userdata = this;
    m_callbacks.channel_open_request_auth_agent_function = &AgentForwardHost::onAuthAgentOpen;

    // Callbacks struct must remain valid for the session lifetime (not copied by libssh).
    if (ssh_set_callbacks(session, &m_callbacks) != SSH_OK) {
        if (errorOut) {
            const char *err = ssh_get_error(session);
            *errorOut =
                err ? QString::fromUtf8(err) : tr("Failed to register agent-forward callbacks");
        }
        m_session = nullptr;
        return false;
    }

    if (ssh_channel_request_auth_agent(firstShellChannel) != SSH_OK) {
        if (errorOut) {
            const char *err = ssh_get_error(session);
            *errorOut = err ? QString::fromUtf8(err) : tr("Failed to request agent forwarding");
        }
        m_session = nullptr;
        return false;
    }

    m_started = true;
    qCWarning(lcSsh) << "Agent forwarding requested on session";
    return true;
}

void AgentForwardHost::poll()
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

void AgentForwardHost::stop()
{
    const QList<TunnelBridge *> bridges = m_bridges;
    for (TunnelBridge *bridge : bridges) {
        closeBridge(bridge);
    }
    m_bridges.clear();
    m_started = false;
    m_session = nullptr;
}

ssh_channel AgentForwardHost::onAuthAgentOpen(ssh_session session, void *userdata)
{
    auto *self = static_cast<AgentForwardHost *>(userdata);
    if (self == nullptr) {
        return nullptr;
    }
    return self->handleAuthAgentOpen(session);
}

ssh_channel AgentForwardHost::handleAuthAgentOpen(ssh_session session)
{
    if (session == nullptr || !m_started) {
        return nullptr;
    }

    ssh_channel channel = ssh_channel_new(session);
    if (channel == nullptr) {
        return nullptr;
    }

    if (!openBridge(channel)) {
        ssh_channel_free(channel);
        return nullptr;
    }

    return channel;
}

bool AgentForwardHost::openBridge(ssh_channel channel)
{
    const QString sockPath = localAgentSocketPath();
    if (sockPath.isEmpty()) {
        qCWarning(lcSsh) << "auth-agent open rejected: SSH_AUTH_SOCK unset";
        return false;
    }

    auto *local = new QLocalSocket(this);
    local->connectToServer(sockPath);
    if (!local->waitForConnected(3000)) {
        qCWarning(lcSsh) << "auth-agent open rejected: cannot connect to" << sockPath
                         << local->errorString();
        local->deleteLater();
        return false;
    }

    auto *bridge = new TunnelBridge;
    bridge->channel = channel;
    bridge->socket = local;
    m_bridges.append(bridge);
    wireBridgeSocket(local);
    return true;
}

void AgentForwardHost::wireBridgeSocket(QIODevice *socket)
{
    connect(socket, &QIODevice::readyRead, this, &AgentForwardHost::onBridgeSocketReadyRead);
    if (auto *local = qobject_cast<QLocalSocket *>(socket)) {
        connect(local,
                &QLocalSocket::disconnected,
                this,
                &AgentForwardHost::onBridgeSocketDisconnected);
    }
}

void AgentForwardHost::onBridgeSocketReadyRead()
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

void AgentForwardHost::onBridgeSocketDisconnected()
{
    auto *device = qobject_cast<QIODevice *>(sender());
    if (TunnelBridge *bridge = bridgeForSocket(device)) {
        closeBridge(bridge);
    }
}

TunnelBridge *AgentForwardHost::bridgeForSocket(QIODevice *socket)
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

void AgentForwardHost::closeBridge(TunnelBridge *bridge)
{
    if (bridge == nullptr) {
        return;
    }
    m_bridges.removeAll(bridge);
    TunnelBridgeIo::closeBridge(bridge, this);
}
