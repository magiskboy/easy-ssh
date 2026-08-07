// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TunnelHostIoHandler.h"

#include "core/ssh/SshIoLoop.h"

TunnelHostIoHandler::TunnelHostIoHandler(Hooks hooks) : m_hooks(std::move(hooks)) {}

TunnelHostIoHandler::~TunnelHostIoHandler()
{
    cancel();
}

QString TunnelHostIoHandler::handlerId()
{
    return QStringLiteral("tunnel-host");
}

QString TunnelHostIoHandler::id() const
{
    return handlerId();
}

bool TunnelHostIoHandler::start(SshIoLoop *loop, QString *error)
{
    if (m_started) {
        return true;
    }
    if (loop == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("TunnelHostIoHandler: missing loop");
        }
        return false;
    }
    m_loop = loop;
    m_started = true;
    m_cancelled = false;
    return true;
}

void TunnelHostIoHandler::cancel()
{
    m_cancelled = true;
    m_loop = nullptr;
}

void TunnelHostIoHandler::onIdle()
{
    if (m_cancelled || !m_started) {
        return;
    }

    if (m_hooks.acceptRemoteForwards) {
        m_hooks.acceptRemoteForwards();
    }

    if (m_hooks.pollKeepAlive) {
        QString error;
        if (!m_hooks.pollKeepAlive(&error)) {
            if (m_hooks.keepaliveFailed) {
                m_hooks.keepaliveFailed(error);
            }
        }
    }
}
