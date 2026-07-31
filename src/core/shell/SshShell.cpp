// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SshShell.h"

#include <QCoreApplication>

namespace
{
QString trShell(const char *text)
{
    return QCoreApplication::translate("SshShell", text);
}
} // namespace

SshShell::~SshShell()
{
    cleanup();
}

bool SshShell::isOpen() const
{
    return m_channel != nullptr && ssh_channel_is_open(m_channel) && !ssh_channel_is_eof(m_channel);
}

QString SshShell::sessionError() const
{
    if (m_session == nullptr) {
        return trShell("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : trShell("Unknown error");
}

void SshShell::cleanup()
{
    if (m_channel) {
        if (ssh_channel_is_open(m_channel)) {
            ssh_channel_send_eof(m_channel);
            ssh_channel_close(m_channel);
        }
        ssh_channel_free(m_channel);
        m_channel = nullptr;
    }
    m_session = nullptr;
}

bool SshShell::open(ssh_session session, int cols, int rows, QString *errorOut)
{
    cleanup();
    m_session = session;

    m_channel = ssh_channel_new(m_session);
    if (m_channel == nullptr) {
        if (errorOut) {
            *errorOut = trShell("Failed to create channel: %1").arg(sessionError());
        }
        return false;
    }

    if (ssh_channel_open_session(m_channel) != SSH_OK) {
        if (errorOut) {
            *errorOut = trShell("Failed to open channel: %1").arg(sessionError());
        }
        cleanup();
        return false;
    }

    if (ssh_channel_request_pty_size(m_channel, "xterm-256color", cols, rows) != SSH_OK) {
        if (errorOut) {
            *errorOut = trShell("Failed to request PTY: %1").arg(sessionError());
        }
        cleanup();
        return false;
    }

    if (ssh_channel_request_shell(m_channel) != SSH_OK) {
        if (errorOut) {
            *errorOut = trShell("Failed to request shell: %1").arg(sessionError());
        }
        cleanup();
        return false;
    }

    return true;
}

bool SshShell::write(const QByteArray &data, QString *errorOut)
{
    if (m_channel == nullptr || data.isEmpty()) {
        return true;
    }

    const char *ptr = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        const int written = ssh_channel_write(m_channel, ptr, static_cast<uint32_t>(remaining));
        if (written == SSH_ERROR || written < 0) {
            if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel) ||
                (m_session && !ssh_is_connected(m_session))) {
                return false;
            }
            if (errorOut) {
                *errorOut = trShell("Failed to write to channel: %1").arg(sessionError());
            }
            return false;
        }
        if (written == 0) {
            break;
        }
        ptr += written;
        remaining -= written;
    }
    return true;
}

bool SshShell::changePtySize(int cols, int rows, QString *errorOut)
{
    Q_UNUSED(errorOut);
    if (m_channel == nullptr) {
        return true;
    }
    if (cols < 2 || rows < 2 || cols > 1000 || rows > 500) {
        return true;
    }
    ssh_channel_change_pty_size(m_channel, cols, rows);
    return true;
}

SshShell::PollStatus SshShell::poll(QByteArray *outData, QString *errorOut)
{
    if (m_session == nullptr) {
        return PollStatus::ChannelClosed;
    }
    if (m_channel == nullptr) {
        return PollStatus::Idle;
    }
    if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel)) {
        return PollStatus::ChannelClosed;
    }

    constexpr int kMaxBytesPerTick = 64 * 1024;
    int totalRead = 0;
    char buffer[4096];
    bool hadData = false;

    auto readStream = [&](int isStderr) -> PollStatus {
        while (totalRead < kMaxBytesPerTick) {
            const int nbytes =
                ssh_channel_read_nonblocking(m_channel, buffer, sizeof(buffer), isStderr);
            if (nbytes == SSH_EOF) {
                return PollStatus::ChannelClosed;
            }
            if (nbytes < 0) {
                if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel)) {
                    return PollStatus::ChannelClosed;
                }
                if (!ssh_is_connected(m_session)) {
                    if (errorOut) {
                        *errorOut = trShell("Read error: %1").arg(sessionError());
                    }
                    return PollStatus::Error;
                }
                if (errorOut) {
                    *errorOut = trShell("Read error: %1").arg(sessionError());
                }
                return PollStatus::Error;
            }
            if (nbytes == 0) {
                break;
            }
            totalRead += nbytes;
            hadData = true;
            if (outData) {
                outData->append(buffer, nbytes);
            }
        }
        return PollStatus::Idle;
    };

    PollStatus st = readStream(0);
    if (st != PollStatus::Idle) {
        return st;
    }
    st = readStream(1);
    if (st != PollStatus::Idle) {
        return st;
    }

    if (!ssh_is_connected(m_session)) {
        if (errorOut) {
            *errorOut = trShell("SSH connection lost");
        }
        return PollStatus::Error;
    }
    if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel)) {
        return PollStatus::ChannelClosed;
    }

    return hadData ? PollStatus::Data : PollStatus::Idle;
}
