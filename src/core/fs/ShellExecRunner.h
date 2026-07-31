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

    bool run(const QString &command, Result *out, QString *error = nullptr);

    static QString stdoutText(const Result &result);
    static QString stderrText(const Result &result);

private:
    QString buildExecCommand(const QString &command) const;
    QString sessionError() const;

    ssh_session m_session = nullptr;
    QString m_shellPath;
};
