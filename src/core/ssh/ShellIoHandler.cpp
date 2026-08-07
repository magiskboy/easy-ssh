/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/ssh/ShellIoHandler.h"

#include "core/ssh/SshIoLoop.h"

ShellIoHandler::ShellIoHandler(
    const QUuid &shellId, ssh_session session, int cols, int rows, Hooks hooks)
    : m_shellId(shellId), m_session(session), m_cols(cols), m_rows(rows), m_hooks(std::move(hooks))
{
}

ShellIoHandler::~ShellIoHandler()
{
    cancel();
}

QString ShellIoHandler::id() const
{
    return m_shellId.toString(QUuid::WithoutBraces);
}

bool ShellIoHandler::start(SshIoLoop *loop, QString *error)
{
    if (m_started) {
        return true;
    }
    if (loop == nullptr || m_session == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("ShellIoHandler: missing loop or session");
        }
        return false;
    }

    m_loop = loop;
    SshShell::AgainPump pump;
    if (m_hooks.againPump) {
        pump = m_hooks.againPump;
    }

    if (!m_shell.open(m_session, m_cols, m_rows, error, pump)) {
        return false;
    }

    if (!loop->registerChannel(m_shell.channel(), this, error)) {
        m_shell.cleanup();
        return false;
    }

    m_started = true;
    m_cancelled = false;
    return true;
}

void ShellIoHandler::cancel()
{
    if (m_cancelled) {
        return;
    }
    m_cancelled = true;

    if (m_loop != nullptr && m_shell.channel() != nullptr) {
        m_loop->unregisterChannel(m_shell.channel());
    }
    m_shell.cleanup();
    m_writeQueue.clear();
    // Do not emit closed here — SshWorker::retireShell owns signal emission.
}

void ShellIoHandler::enqueueWrite(const QByteArray &data)
{
    if (data.isEmpty() || m_cancelled || m_remoteClosed) {
        return;
    }
    m_writeQueue.append(data);
    if (m_loop != nullptr) {
        m_loop->wake();
    }
}

bool ShellIoHandler::changePtySize(int cols, int rows, QString *errorOut)
{
    m_cols = cols;
    m_rows = rows;
    return m_shell.changePtySize(cols, rows, errorOut);
}

int ShellIoHandler::onData(
    ssh_session session, ssh_channel channel, void *data, uint32_t len, int isStderr)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    Q_UNUSED(isStderr);
    if (data != nullptr && len > 0) {
        m_pendingOut.append(static_cast<const char *>(data), static_cast<int>(len));
    }
    // Fully consume to avoid libssh delayed-close stalls.
    return static_cast<int>(len);
}

void ShellIoHandler::onEof(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_remoteClosed = true;
}

void ShellIoHandler::onClose(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_remoteClosed = true;
}

void ShellIoHandler::onIdle()
{
    if (m_cancelled) {
        return;
    }

    deliverPendingOutput();
    flushWriteQueue();
    finishIfNeeded();
}

void ShellIoHandler::deliverPendingOutput()
{
    if (m_pendingOut.isEmpty()) {
        return;
    }
    const QByteArray chunk = std::move(m_pendingOut);
    m_pendingOut.clear();
    if (m_hooks.dataReady) {
        m_hooks.dataReady(m_shellId, chunk);
    }
}

void ShellIoHandler::flushWriteQueue()
{
    if (m_writeQueue.isEmpty() || m_cancelled || m_remoteClosed) {
        return;
    }

    while (!m_writeQueue.isEmpty()) {
        QString error;
        const int written =
            m_shell.writeNonBlocking(m_writeQueue.constData(), m_writeQueue.size(), &error);
        if (written < 0) {
            m_writeFailed = true;
            m_writeError = error;
            m_writeQueue.clear();
            m_remoteClosed = true;
            return;
        }
        if (written == 0) {
            return; // would block — retry next idle
        }
        m_writeQueue.remove(0, written);
    }
}

void ShellIoHandler::finishIfNeeded()
{
    if (m_finished || !m_remoteClosed) {
        return;
    }
    deliverPendingOutput();
    m_finished = true;

    if (m_writeFailed && m_hooks.failed) {
        m_hooks.failed(m_shellId,
                       m_writeError.isEmpty() ? QStringLiteral("Shell write error") : m_writeError);
    }
    if (m_hooks.closed) {
        m_hooks.closed(m_shellId);
    }
}
