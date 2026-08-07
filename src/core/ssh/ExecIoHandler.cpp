// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/ssh/ExecIoHandler.h"

#include "core/ssh/SshIoLoop.h"

#include <QCoreApplication>

namespace
{
QString trExec(const char *text)
{
    return QCoreApplication::translate("ExecIoHandler", text);
}
} // namespace

ExecIoHandler::ExecIoHandler(ssh_session session, QString requestId, QString command, Hooks hooks)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_session(session),
      m_requestId(std::move(requestId)), m_command(std::move(command)), m_hooks(std::move(hooks))
{
}

ExecIoHandler::~ExecIoHandler()
{
    cancel();
}

QString ExecIoHandler::id() const
{
    return m_id;
}

void ExecIoHandler::setCompletedHook(std::function<void()> completed)
{
    m_hooks.completed = std::move(completed);
}

QString ExecIoHandler::sessionError() const
{
    if (m_session == nullptr) {
        return trExec("No SSH session");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : trExec("Unknown SSH error");
}

bool ExecIoHandler::start(SshIoLoop *loop, QString *error)
{
    if (m_started) {
        return true;
    }
    if (loop == nullptr || m_session == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("ExecIoHandler: missing loop or session");
        }
        return false;
    }
    if (m_command.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = trExec("Empty remote command");
        }
        return false;
    }

    m_channel = ssh_channel_new(m_session);
    if (m_channel == nullptr) {
        if (error != nullptr) {
            *error = trExec("Failed to create channel: %1").arg(sessionError());
        }
        return false;
    }

    if (!loop->registerChannel(m_channel, this, error)) {
        ssh_channel_free(m_channel);
        m_channel = nullptr;
        return false;
    }

    m_loop = loop;
    m_started = true;
    m_cancelled = false;
    m_state = State::Opening;
    m_timer.start();
    // Defer open/exec to onIdle so completed hooks cannot race addHandler.
    m_loop->wake();
    return true;
}

void ExecIoHandler::cancel()
{
    if (m_cancelled) {
        return;
    }
    m_cancelled = true;
    cleanupChannel();
    if (!m_finished && m_started) {
        finishFail(trExec("Remote command canceled"));
    } else {
        m_finished = true;
        m_state = State::Done;
    }
}

void ExecIoHandler::cleanupChannel()
{
    if (m_channel == nullptr) {
        return;
    }
    if (m_loop != nullptr) {
        m_loop->unregisterChannel(m_channel);
    }
    if (ssh_channel_is_open(m_channel)) {
        ssh_channel_send_eof(m_channel);
        ssh_channel_close(m_channel);
    }
    ssh_channel_free(m_channel);
    m_channel = nullptr;
}

void ExecIoHandler::advanceOpening()
{
    if (m_channel == nullptr || m_state != State::Opening) {
        return;
    }
    const int rc = ssh_channel_open_session(m_channel);
    if (rc == SSH_AGAIN) {
        return;
    }
    if (rc != SSH_OK) {
        finishFail(trExec("Failed to open channel: %1").arg(sessionError()));
        return;
    }
    m_state = State::Executing;
    advanceExecuting();
}

void ExecIoHandler::advanceExecuting()
{
    if (m_channel == nullptr || m_state != State::Executing) {
        return;
    }
    const QByteArray cmd = m_command.toUtf8();
    const int rc = ssh_channel_request_exec(m_channel, cmd.constData());
    if (rc == SSH_AGAIN) {
        return;
    }
    if (rc != SSH_OK) {
        finishFail(trExec("Failed to execute remote command: %1").arg(sessionError()));
        return;
    }
    m_state = State::Reading;
}

void ExecIoHandler::onIdle()
{
    if (!m_started || m_finished || m_cancelled) {
        return;
    }

    if (m_timer.isValid() && m_timer.elapsed() >= kTimeoutMs) {
        finishFail(trExec("Remote command timed out"));
        return;
    }

    switch (m_state) {
    case State::Opening:
        advanceOpening();
        break;
    case State::Executing:
        advanceExecuting();
        break;
    case State::Reading:
        tryFinish();
        break;
    case State::Idle:
    case State::Done:
        break;
    }

    if (!m_finished && m_loop != nullptr &&
        (m_state == State::Opening || m_state == State::Executing)) {
        m_loop->wake();
    }
}

int ExecIoHandler::onData(
    ssh_session session, ssh_channel channel, void *data, uint32_t len, int isStderr)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    if (data != nullptr && len > 0) {
        if (isStderr) {
            m_stderr.append(static_cast<const char *>(data), static_cast<int>(len));
        } else {
            m_stdout.append(static_cast<const char *>(data), static_cast<int>(len));
        }
    }
    return static_cast<int>(len);
}

void ExecIoHandler::onEof(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_eof = true;
}

void ExecIoHandler::onClose(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_closed = true;
    m_eof = true;
}

void ExecIoHandler::onExitStatus(ssh_session session, ssh_channel channel, int exitStatus)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_exitStatus = exitStatus;
    m_exitSeen = true;
}

void ExecIoHandler::tryFinish()
{
    if (m_finished || m_state != State::Reading) {
        return;
    }
    if (!m_eof && !m_closed) {
        return;
    }

    if (!m_exitSeen && m_channel != nullptr) {
        uint32_t exitStatus = 0;
        char *exitSignal = nullptr;
        int coreDumped = 0;
        const int rc = ssh_channel_get_exit_state(m_channel, &exitStatus, &exitSignal, &coreDumped);
        if (exitSignal != nullptr) {
            ssh_string_free_char(exitSignal);
        }
        if (rc == SSH_OK) {
            m_exitStatus = static_cast<int>(exitStatus);
            m_exitSeen = true;
        } else if (!m_closed) {
            // Wait for exit-status callback or channel close.
            return;
        }
    }

    finishOk();
}

void ExecIoHandler::finishOk()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_state = State::Done;
    cleanupChannel();
    if (m_hooks.finished) {
        m_hooks.finished(m_requestId, m_exitStatus, m_stdout, m_stderr, QString());
    }
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}

void ExecIoHandler::finishFail(const QString &error)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_state = State::Done;
    cleanupChannel();
    if (m_hooks.finished) {
        m_hooks.finished(m_requestId, m_exitStatus, m_stdout, m_stderr, error);
    }
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}
