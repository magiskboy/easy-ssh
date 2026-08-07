/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>

#include <libssh/libssh.h>

class SshIoLoop;

/**
 * Base for I/O ops registered on a SshIoLoop (shell, exec, SFTP, tunnel, …).
 *
 * Threading: start / cancel / onIdle are invoked only on the loop/worker thread.
 * Callbacks from libssh must only enqueue work; do heavy work in onIdle().
 */
class SshIoHandler
{
public:
    virtual ~SshIoHandler() = default;

    virtual QString id() const = 0;

    /// Called once when added to the loop. Return false and set @p error on failure.
    virtual bool start(SshIoLoop *loop, QString *error) = 0;

    /// Abort in-flight work; must be idempotent.
    virtual void cancel() = 0;

    /// Called after each ssh_event_dopoll return (SFTP AIO / deferred work).
    virtual void onIdle() {}
};

/**
 * Sink for ssh_channel callbacks installed via SshIoLoop::registerChannel.
 * Keep instances alive until unregisterChannel / channel close / detachSession.
 */
class SshChannelCallbacks
{
public:
    virtual ~SshChannelCallbacks() = default;

    /// Return bytes consumed (typically @p len). Called from libssh during dopoll.
    virtual int
    onData(ssh_session session, ssh_channel channel, void *data, uint32_t len, int isStderr) = 0;
    virtual void onEof(ssh_session session, ssh_channel channel) {}
    virtual void onClose(ssh_session session, ssh_channel channel) {}
    virtual void onExitStatus(ssh_session /*session*/, ssh_channel /*channel*/, int /*exitStatus*/)
    {
    }
};
