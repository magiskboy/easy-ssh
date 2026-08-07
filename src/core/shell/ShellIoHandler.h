/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/shell/SshShell.h"
#include "core/ssh/SshIoHandler.h"

#include <QByteArray>
#include <QString>
#include <QUuid>

#include <functional>

class SshIoLoop;

/**
 * One interactive PTY shell on a SshIoLoop (Phase 2).
 * Channel callbacks enqueue data; onIdle flushes writes and delivers output.
 */
class ShellIoHandler final : public SshIoHandler, public SshChannelCallbacks
{
public:
    struct Hooks
    {
        std::function<void(const QUuid &shellId, const QByteArray &data)> dataReady;
        std::function<void(const QUuid &shellId)> closed;
        std::function<void(const QUuid &shellId, const QString &message)> failed;
        /// Return false to abort SSH_AGAIN waits during open.
        std::function<bool()> againPump;
    };

    ShellIoHandler(const QUuid &shellId, ssh_session session, int cols, int rows, Hooks hooks);
    ~ShellIoHandler() override;

    QString id() const override;
    QUuid shellId() const { return m_shellId; }
    SshShell *shell() { return &m_shell; }
    ssh_channel channel() const { return m_shell.channel(); }

    bool start(SshIoLoop *loop, QString *error) override;
    void cancel() override;
    void onIdle() override;

    void enqueueWrite(const QByteArray &data);
    bool changePtySize(int cols, int rows, QString *errorOut = nullptr);

    // SshChannelCallbacks
    int onData(
        ssh_session session, ssh_channel channel, void *data, uint32_t len, int isStderr) override;
    void onEof(ssh_session session, ssh_channel channel) override;
    void onClose(ssh_session session, ssh_channel channel) override;

private:
    void flushWriteQueue();
    void deliverPendingOutput();
    void finishIfNeeded();

    QUuid m_shellId;
    ssh_session m_session = nullptr;
    int m_cols = 80;
    int m_rows = 24;
    Hooks m_hooks;
    SshShell m_shell;
    SshIoLoop *m_loop = nullptr;
    QByteArray m_pendingOut;
    QByteArray m_writeQueue;
    bool m_started = false;
    bool m_cancelled = false;
    bool m_remoteClosed = false;
    bool m_finished = false;
    bool m_writeFailed = false;
    QString m_writeError;
};
