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

    /// Optional hook invoked while waiting for remote output so the caller can
    /// keep polling other SSH channels (shell / tunnels) on the same session.
    void setPump(std::function<void()> pump) { m_pump = std::move(pump); }

    bool run(const QString &command, Result *out, QString *error = nullptr);

    static QString stdoutText(const Result &result);
    static QString stderrText(const Result &result);

private:
    QString buildExecCommand(const QString &command) const;
    QString sessionError() const;
    void pump() const;
    bool waitChannelOk(const std::function<int()> &op,
                       Result *result,
                       QString *error,
                       const char *failPrefix);

    ssh_session m_session = nullptr;
    QString m_shellPath;
    std::function<void()> m_pump;
};
