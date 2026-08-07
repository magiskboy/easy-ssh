/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/ssh/SshIoHandler.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QUuid>

#include <functional>

#include <libssh/libssh.h>

class SshIoLoop;

/**
 * One-shot remote exec (no PTY) on SshIoLoop (Phase 4).
 * Channel callbacks enqueue data; onIdle advances open/exec and finishes.
 */
class ExecIoHandler final : public SshIoHandler, public SshChannelCallbacks
{
public:
    static constexpr int kTimeoutMs = 120000;

    struct Hooks
    {
        std::function<void(const QString &requestId,
                           int exitStatus,
                           const QByteArray &stdoutBytes,
                           const QByteArray &stderrBytes,
                           const QString &errorMessage)>
            finished;
        std::function<void()> completed;
    };

    ExecIoHandler(ssh_session session, QString requestId, QString command, Hooks hooks);
    ~ExecIoHandler() override;

    QString id() const override;
    QString requestId() const { return m_requestId; }
    void setCompletedHook(std::function<void()> completed);

    bool start(SshIoLoop *loop, QString *error) override;
    void cancel() override;
    void onIdle() override;

    // SshChannelCallbacks
    int onData(
        ssh_session session, ssh_channel channel, void *data, uint32_t len, int isStderr) override;
    void onEof(ssh_session session, ssh_channel channel) override;
    void onClose(ssh_session session, ssh_channel channel) override;
    void onExitStatus(ssh_session session, ssh_channel channel, int exitStatus) override;

private:
    enum class State : uint8_t
    {
        Idle,
        Opening,
        Executing,
        Reading,
        Done,
    };

    void advanceOpening();
    void advanceExecuting();
    void tryFinish();
    void finishOk();
    void finishFail(const QString &error);
    void cleanupChannel();
    QString sessionError() const;

    QString m_id;
    ssh_session m_session = nullptr;
    QString m_requestId;
    QString m_command;
    Hooks m_hooks;
    SshIoLoop *m_loop = nullptr;
    ssh_channel m_channel = nullptr;
    State m_state = State::Idle;
    QByteArray m_stdout;
    QByteArray m_stderr;
    int m_exitStatus = -1;
    bool m_exitSeen = false;
    bool m_eof = false;
    bool m_closed = false;
    bool m_started = false;
    bool m_cancelled = false;
    bool m_finished = false;
    QElapsedTimer m_timer;
};
