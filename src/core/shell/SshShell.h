/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <libssh/libssh.h>

/**
 * Interactive shell channel over an established ssh_session (PTY + shell).
 */
class SshShell
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

    SshShell() = default;
    ~SshShell();

    SshShell(const SshShell &) = delete;
    SshShell &operator=(const SshShell &) = delete;

    bool open(ssh_session session, int cols, int rows, QString *errorOut = nullptr);
    void cleanup();

    bool isOpen() const;
    ssh_channel channel() const { return m_channel; }

    bool write(const QByteArray &data, QString *errorOut = nullptr);
    bool changePtySize(int cols, int rows, QString *errorOut = nullptr);
    PollStatus poll(QByteArray *outData, QString *errorOut);

private:
    QString sessionError() const;

    ssh_session m_session = nullptr;
    ssh_channel m_channel = nullptr;
};
