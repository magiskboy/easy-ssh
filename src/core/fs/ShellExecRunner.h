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
 * One-shot remote command via ssh_channel_request_exec (no PTY).
 * Optional wrapper: <shell> -c '<command>' when shell path is set.
 *
 * Used by ScpEngine open/probe (connect-time) and sync helpers. Hot-path browse
 * uses ScpMetaIoHandler / ExecIoHandler on SshIoLoop instead.
 */
class ShellExecRunner
{
public:
    struct Result
    {
        int exitStatus = -1;
        QByteArray stdoutBytes;
        QByteArray stderrBytes;
        QString errorMessage;
    };

    explicit ShellExecRunner(ssh_session session, QString shellPath = {});

    void setShellPath(const QString &shellPath);
    QString shellPath() const { return m_shellPath; }

    /// Wrap @p command for @p shellPath -c '…' when shell is set; otherwise return as-is.
    static QString wrapCommand(const QString &shellPath, const QString &command);

    bool run(const QString &command, Result *out, QString *error = nullptr);

    static QString stdoutText(const Result &result);
    static QString stderrText(const Result &result);

private:
    QString buildExecCommand(const QString &command) const;
    QString sessionError() const;
    bool waitChannelOk(const std::function<int()> &op,
                       Result *result,
                       QString *error,
                       const char *failPrefix);

    ssh_session m_session = nullptr;
    QString m_shellPath;
};
