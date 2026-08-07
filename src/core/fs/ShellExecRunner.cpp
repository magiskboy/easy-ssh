// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ShellExecRunner.h"

#include "ShellCommandSet.h"

#include <QCoreApplication>
#include <QScopeGuard>
#include <QThread>

namespace
{
QString trExec(const char *text)
{
    return QCoreApplication::translate("ShellExecRunner", text);
}

constexpr int kPollSleepMs = 5;
constexpr int kMaxWaitMs = 120000;
} // namespace

ShellExecRunner::ShellExecRunner(ssh_session session, QString shellPath)
    : m_session(session), m_shellPath(std::move(shellPath))
{
}

void ShellExecRunner::setShellPath(const QString &shellPath)
{
    m_shellPath = shellPath;
}

QString ShellExecRunner::sessionError() const
{
    if (m_session == nullptr) {
        return trExec("No SSH session");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : trExec("Unknown SSH error");
}

QString ShellExecRunner::buildExecCommand(const QString &command) const
{
    return wrapCommand(m_shellPath, command);
}

QString ShellExecRunner::wrapCommand(
    const QString &shellPath, // NOLINT(bugprone-easily-swappable-parameters)
    const QString &command)   // NOLINT(bugprone-easily-swappable-parameters)
{
    const QString shell = shellPath.trimmed();
    if (shell.isEmpty()) {
        return command;
    }
    return shell + QStringLiteral(" -c ") + ShellCommandSet::shellQuote(command);
}

QString ShellExecRunner::stdoutText(const Result &result)
{
    return QString::fromUtf8(result.stdoutBytes);
}

QString ShellExecRunner::stderrText(const Result &result)
{
    return QString::fromUtf8(result.stderrBytes);
}

bool ShellExecRunner::waitChannelOk(const std::function<int()> &op,
                                    Result *result,
                                    QString *error,
                                    const char *failPrefix)
{
    int spins = 0;
    for (;;) {
        const int rc = op();
        if (rc == SSH_OK) {
            return true;
        }
        if (rc == SSH_AGAIN) {
            if (++spins > 2000) {
                result->errorMessage = trExec("%1: timed out").arg(QString::fromUtf8(failPrefix));
                if (error) {
                    *error = result->errorMessage;
                }
                return false;
            }
            QThread::msleep(static_cast<unsigned long>(kPollSleepMs));
            continue;
        }
        result->errorMessage = trExec("%1: %2").arg(QString::fromUtf8(failPrefix), sessionError());
        if (error) {
            *error = result->errorMessage;
        }
        return false;
    }
}

bool ShellExecRunner::run(const QString &command, Result *out, QString *error)
{
    Result local;
    Result *result = out ? out : &local;
    *result = Result{};

    if (m_session == nullptr) {
        result->errorMessage = trExec("No SSH session");
        if (error) {
            *error = result->errorMessage;
        }
        return false;
    }

    // Sync exec assumes a blocking session. IoLoop keeps the session
    // non-blocking — temporarily restore blocking for this one-shot channel
    // (connect-time SCP probe / open only).
    const int wasBlocking = ssh_is_blocking(m_session);
    ssh_set_blocking(m_session, 1);
    auto restoreBlocking = qScopeGuard([this, wasBlocking]() {
        if (m_session != nullptr) {
            ssh_set_blocking(m_session, wasBlocking);
        }
    });

    ssh_channel channel = ssh_channel_new(m_session);
    if (channel == nullptr) {
        result->errorMessage = trExec("Failed to create channel: %1").arg(sessionError());
        if (error) {
            *error = result->errorMessage;
        }
        return false;
    }

    auto cleanup = [&]() {
        if (channel) {
            if (ssh_channel_is_open(channel)) {
                ssh_channel_send_eof(channel);
                ssh_channel_close(channel);
            }
            ssh_channel_free(channel);
            channel = nullptr;
        }
    };

    if (!waitChannelOk([&]() { return ssh_channel_open_session(channel); },
                       result,
                       error,
                       "Failed to open channel")) {
        cleanup();
        return false;
    }

    const QByteArray execCmd = buildExecCommand(command).toUtf8();
    if (!waitChannelOk([&]() { return ssh_channel_request_exec(channel, execCmd.constData()); },
                       result,
                       error,
                       "Failed to execute remote command")) {
        cleanup();
        return false;
    }

    char buffer[16384];
    int waitedMs = 0;
    bool eof = false;
    while (!eof && waitedMs < kMaxWaitMs) {
        const int nout = ssh_channel_read_timeout(channel, buffer, sizeof(buffer), 0, kPollSleepMs);
        if (nout > 0) {
            result->stdoutBytes.append(buffer, nout);
            continue;
        }
        if (nout == SSH_ERROR) {
            result->errorMessage = trExec("Failed to read command output: %1").arg(sessionError());
            if (error) {
                *error = result->errorMessage;
            }
            cleanup();
            return false;
        }

        const int nerr = ssh_channel_read_timeout(channel, buffer, sizeof(buffer), 1, kPollSleepMs);
        if (nerr > 0) {
            result->stderrBytes.append(buffer, nerr);
            continue;
        }
        if (nerr == SSH_ERROR) {
            result->errorMessage = trExec("Failed to read command stderr: %1").arg(sessionError());
            if (error) {
                *error = result->errorMessage;
            }
            cleanup();
            return false;
        }

        if (ssh_channel_is_eof(channel)) {
            eof = true;
            break;
        }
        QThread::msleep(static_cast<unsigned long>(kPollSleepMs));
        waitedMs += kPollSleepMs;
    }

    // Drain remaining
    for (;;) {
        const int nout = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
        if (nout > 0) {
            result->stdoutBytes.append(buffer, nout);
            continue;
        }
        break;
    }
    for (;;) {
        const int nerr = ssh_channel_read(channel, buffer, sizeof(buffer), 1);
        if (nerr > 0) {
            result->stderrBytes.append(buffer, nerr);
            continue;
        }
        break;
    }

    result->exitStatus = ssh_channel_get_exit_status(channel);
    cleanup();

    if (waitedMs >= kMaxWaitMs && !eof) {
        result->errorMessage = trExec("Remote command timed out");
        if (error) {
            *error = result->errorMessage;
        }
        return false;
    }

    return true;
}
