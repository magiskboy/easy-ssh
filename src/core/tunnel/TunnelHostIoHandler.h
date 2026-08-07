/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/ssh/SshIoHandler.h"

#include <QString>

#include <functional>

class SshIoLoop;

/**
 * Session-level IoLoop handler: remote-forward accept + keepalive onIdle.
 * One instance per SshWorker while the IoLoop is attached.
 */
class TunnelHostIoHandler final : public SshIoHandler
{
public:
    struct Hooks
    {
        std::function<void()> acceptRemoteForwards;
        /// Return false and set @p error on fatal keepalive failure.
        std::function<bool(QString *error)> pollKeepAlive;
        std::function<void(const QString &error)> keepaliveFailed;
    };

    explicit TunnelHostIoHandler(Hooks hooks);
    ~TunnelHostIoHandler() override;

    QString id() const override;
    bool start(SshIoLoop *loop, QString *error) override;
    void cancel() override;
    void onIdle() override;

    static QString handlerId();

private:
    Hooks m_hooks;
    SshIoLoop *m_loop = nullptr;
    bool m_started = false;
    bool m_cancelled = false;
};
