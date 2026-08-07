/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

#include <libssh/libssh.h>

/**
 * Interactive shell channel over an established ssh_session (PTY + shell).
 */
class SshTerminal
{
public:
    enum class PollStatus
    {
        Idle,
        Data,
        /// Channel EOF / closed — not transport loss; caller must not tear down ssh_session.
        ChannelClosed,
        Error,
    };

    /// Optional pump invoked on SSH_AGAIN during open (e.g. SshIoLoop::pollOnce).
    /// Return false to abort.
    using AgainPump = std::function<bool()>;

    SshTerminal() = default;
    ~SshTerminal();

    SshTerminal(const SshTerminal &) = delete;
    SshTerminal &operator=(const SshTerminal &) = delete;

    /// Optional hook after open+PTY and before request_shell (e.g. ForwardAgent).
    /// Return false to abort open (errorOut may be filled by the hook).
    using BeforeTerminalHook = std::function<bool(ssh_channel channel, QString *errorOut)>;

    bool open(ssh_session session,
              int cols,
              int rows,
              QString *errorOut = nullptr,
              const AgainPump &againPump = {},
              const BeforeTerminalHook &beforeTerminal = {});
    void cleanup();

    bool isOpen() const;
    ssh_channel channel() const { return m_channel; }

    /// Blocking-style write: loops until all data is sent (or error). Prefer
    /// writeNonBlocking under a non-blocking session with a write queue.
    bool write(const QByteArray &data, QString *errorOut = nullptr);

    /// Write as much as possible without blocking. Returns bytes written (>=0),
    /// or -1 on hard error. SSH_AGAIN / would-block yields 0 with empty error.
    int writeNonBlocking(const char *data, int len, QString *errorOut = nullptr);

    bool changePtySize(int cols, int rows, QString *errorOut = nullptr);
    PollStatus poll(QByteArray *outData, QString *errorOut);

private:
    QString sessionError() const;
    bool pumpAgain(const AgainPump &againPump, QString *errorOut, const char *what);

    ssh_session m_session = nullptr;
    ssh_channel m_channel = nullptr;
};
