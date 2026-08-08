/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/terminal/TerminalIoHandler.h"

#include "core/ssh/SshIoLoop.h"

TerminalIoHandler::TerminalIoHandler(const QUuid &terminalId,
                                     ssh_session session,
                                     int cols, // NOLINT(bugprone-easily-swappable-parameters)
                                     int rows, // NOLINT(bugprone-easily-swappable-parameters)
                                     Hooks hooks)
    : m_terminalId(terminalId), m_session(session), m_cols(cols), m_rows(rows),
      m_hooks(std::move(hooks))
{
}

TerminalIoHandler::~TerminalIoHandler()
{
    cancel();
}

QString TerminalIoHandler::id() const
{
    return m_terminalId.toString(QUuid::WithoutBraces);
}

bool TerminalIoHandler::start(SshIoLoop *loop, QString *error)
{
    if (m_started) {
        return true;
    }
    if (loop == nullptr || m_session == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("TerminalIoHandler: missing loop or session");
        }
        return false;
    }

    m_loop = loop;
    SshTerminal::AgainPump pump;
    if (m_hooks.againPump) {
        pump = m_hooks.againPump;
    }

    SshTerminal::BeforeTerminalHook beforeTerminal;
    if (m_hooks.beforeTerminal) {
        beforeTerminal = m_hooks.beforeTerminal;
    }

    if (!m_terminal.open(m_session, m_cols, m_rows, error, pump, beforeTerminal)) {
        return false;
    }

    if (!loop->registerChannel(m_terminal.channel(), this, error)) {
        m_terminal.cleanup();
        return false;
    }

    m_started = true;
    m_cancelled = false;
    return true;
}

void TerminalIoHandler::cancel()
{
    if (m_cancelled) {
        return;
    }
    m_cancelled = true;

    if (m_loop != nullptr && m_terminal.channel() != nullptr) {
        m_loop->unregisterChannel(m_terminal.channel());
    }
    m_terminal.cleanup();
    m_writeQueue.clear();
    // Do not emit closed here — SshWorker::retireTerminal owns signal emission.
}

void TerminalIoHandler::enqueueWrite(const QByteArray &data)
{
    if (data.isEmpty() || m_cancelled || m_remoteClosed) {
        return;
    }
    m_writeQueue.append(data);
    if (m_loop != nullptr) {
        m_loop->wake();
    }
}

bool TerminalIoHandler::changePtySize(int cols, int rows, QString *errorOut)
{
    m_cols = cols;
    m_rows = rows;
    return m_terminal.changePtySize(cols, rows, errorOut);
}

int TerminalIoHandler::onData(ssh_session session,
                              ssh_channel channel,
                              void *data,
                              uint32_t len, // NOLINT(bugprone-easily-swappable-parameters)
                              int isStderr) // NOLINT(bugprone-easily-swappable-parameters)
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

void TerminalIoHandler::onEof(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_remoteClosed = true;
}

void TerminalIoHandler::onClose(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_remoteClosed = true;
}

void TerminalIoHandler::onIdle()
{
    if (m_cancelled) {
        return;
    }

    deliverPendingOutput();
    flushWriteQueue();
    finishIfNeeded();
}

void TerminalIoHandler::deliverPendingOutput()
{
    if (m_pendingOut.isEmpty()) {
        return;
    }
    const QByteArray chunk = std::move(m_pendingOut);
    m_pendingOut.clear();
    if (m_hooks.dataReady) {
        m_hooks.dataReady(m_terminalId, chunk);
    }
}

void TerminalIoHandler::flushWriteQueue()
{
    if (m_writeQueue.isEmpty() || m_cancelled || m_remoteClosed) {
        return;
    }

    while (!m_writeQueue.isEmpty()) {
        QString error;
        const int written =
            m_terminal.writeNonBlocking(m_writeQueue.constData(), m_writeQueue.size(), &error);
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

void TerminalIoHandler::finishIfNeeded()
{
    if (m_finished || !m_remoteClosed) {
        return;
    }
    deliverPendingOutput();
    m_finished = true;

    if (m_writeFailed && m_hooks.failed) {
        m_hooks.failed(m_terminalId,
                       m_writeError.isEmpty() ? QStringLiteral("Terminal write error")
                                              : m_writeError);
    }
    if (m_hooks.closed) {
        m_hooks.closed(m_terminalId);
    }
}
