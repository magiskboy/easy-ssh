// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ScpMetaIoHandler.h"

#include "ScpEngine.h"
#include "ShellCommandSet.h"
#include "ShellExecRunner.h"
#include "Symlink.h"
#include "core/ssh/SshIoLoop.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>

namespace
{
constexpr int kTimeoutMs = 120000;

QString trMeta(const char *text)
{
    return QCoreApplication::translate("SshWorker", text);
}

void sortEntries(QVector<RemoteEntry> *entries)
{
    if (entries == nullptr) {
        return;
    }
    std::sort(entries->begin(), entries->end(), [](const RemoteEntry &a, const RemoteEntry &b) {
        const bool aDir = Symlink::isDirectoryLike(a);
        const bool bDir = Symlink::isDirectoryLike(b);
        if (aDir != bDir) {
            return aDir;
        }
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
}

QString
detailFromResult(int exitStatus, const QByteArray &stdoutBytes, const QByteArray &stderrBytes)
{
    QString detail = QString::fromUtf8(stderrBytes).trimmed();
    if (detail.isEmpty()) {
        detail = QString::fromUtf8(stdoutBytes).trimmed();
    }
    if (detail.isEmpty()) {
        return trMeta("Remote command failed (exit %1)").arg(exitStatus);
    }
    return detail;
}
} // namespace

ScpMetaIoHandler::ScpMetaIoHandler(FsRemote *fs, Request request, Hooks hooks)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_fs(fs),
      m_request(std::move(request)), m_hooks(std::move(hooks))
{
}

ScpMetaIoHandler::~ScpMetaIoHandler()
{
    cancel();
}

QString ScpMetaIoHandler::id() const
{
    return m_id;
}

void ScpMetaIoHandler::setCompletedHook(std::function<void()> completed)
{
    m_hooks.completed = std::move(completed);
}

QString ScpMetaIoHandler::sessionError() const
{
    if (m_session == nullptr) {
        return trMeta("No SSH session");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : trMeta("Unknown SSH error");
}

QString ScpMetaIoHandler::wrap(const QString &command) const
{
    if (m_fs == nullptr) {
        return command;
    }
    return ShellExecRunner::wrapCommand(m_fs->shellCommands().shell, command);
}

bool ScpMetaIoHandler::start(SshIoLoop *loop, QString *error)
{
    if (m_started) {
        return true;
    }
    if (loop == nullptr || m_fs == nullptr || !m_fs->isOpen() || m_fs->scpEngine() == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("ScpMetaIoHandler: missing loop or SCP backend");
        }
        return false;
    }
    m_session = m_fs->sshSession();
    if (m_session == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("ScpMetaIoHandler: missing ssh session");
        }
        return false;
    }
    m_loop = loop;
    m_allowWarn = m_fs->shellCommands().ignoreLsWarnings;
    m_started = true;
    m_cancelled = false;
    m_phase = Phase::Plan;
    m_loop->wake();
    return true;
}

void ScpMetaIoHandler::cancel()
{
    if (m_cancelled) {
        return;
    }
    m_cancelled = true;
    cleanupChannel();
    if (!m_finished && m_started) {
        m_finished = true;
        if (m_hooks.completed) {
            m_hooks.completed();
        }
    } else {
        m_finished = true;
    }
}

void ScpMetaIoHandler::finishOk()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_phase = Phase::Done;
    cleanupChannel();
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}

void ScpMetaIoHandler::finishFail(const QString &message)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_phase = Phase::Done;
    cleanupChannel();
    if (m_hooks.failed) {
        m_hooks.failed(message);
    }
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}

void ScpMetaIoHandler::cleanupChannel()
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
    m_execState = ExecState::Idle;
}

void ScpMetaIoHandler::planSteps()
{
    m_steps.clear();
    ScpEngine *scp = m_fs->scpEngine();
    if (scp == nullptr) {
        finishFail(trMeta("SCP backend is not available"));
        return;
    }

    ShellCommandSet &cmds = scp->commandSet();

    switch (m_request.op) {
    case Op::CreateDirectory:
        m_steps.push_back(
            CmdStep{cmds.formatMkdir(m_request.path), false, CmdStep::Kind::ShortOk, {}});
        break;
    case Op::CreateSymlink:
        m_steps.push_back(CmdStep{cmds.formatSymlink(m_request.target, m_request.linkPath),
                                  false,
                                  CmdStep::Kind::ShortOk,
                                  {}});
        break;
    case Op::RenamePath:
        m_steps.push_back(CmdStep{
            cmds.formatRename(m_request.from, m_request.to), false, CmdStep::Kind::ShortOk, {}});
        break;
    case Op::RemovePath:
        m_steps.push_back(
            CmdStep{cmds.formatRemove(m_request.path), false, CmdStep::Kind::ShortOk, {}});
        break;
    case Op::CanonicalizePath: {
        const QString requested = m_request.path.isEmpty() ? QStringLiteral(".") : m_request.path;
        m_request.path = requested;
        m_steps.push_back(
            CmdStep{cmds.formatRealpath(requested), false, CmdStep::Kind::CanonicalPrimary, {}});
        break;
    }
    case Op::ResolveEntry:
        m_steps.push_back(CmdStep{
            cmds.formatTestDirectory(m_request.path), false, CmdStep::Kind::ResolveTestDir, {}});
        break;
    case Op::ListDirectory: {
        const QString listPath = Symlink::directoryListPath(m_request.path);
        if (!cmds.config().tryFullTime) {
            scp->setFullTimeProbed(true, false);
            m_steps.push_back(CmdStep{cmds.formatListDirectory(listPath, scp->lsOptions()),
                                      m_allowWarn,
                                      CmdStep::Kind::ListMain,
                                      {}});
        } else if (!scp->fullTimeProbed()) {
            m_steps.push_back(
                CmdStep{cmds.formatListDirectory(listPath, QStringLiteral("--full-time")),
                        false,
                        CmdStep::Kind::ListProbeFullTime,
                        {}});
        } else {
            m_steps.push_back(CmdStep{cmds.formatListDirectory(listPath, scp->lsOptions()),
                                      m_allowWarn,
                                      CmdStep::Kind::ListMain,
                                      {}});
        }
        break;
    }
    }

    startNextCmd();
}

void ScpMetaIoHandler::startNextCmd()
{
    if (m_cancelled || m_finished) {
        return;
    }
    if (m_steps.empty()) {
        // Completed multi-step without explicit finish — should not happen.
        finishOk();
        return;
    }

    m_current = m_steps.front();
    m_steps.pop_front();

    cleanupChannel();
    m_stdout.clear();
    m_stderr.clear();
    m_exitStatus = -1;
    m_exitSeen = false;
    m_eof = false;
    m_closed = false;

    m_channel = ssh_channel_new(m_session);
    if (m_channel == nullptr) {
        finishFail(trMeta("Failed to create channel: %1").arg(sessionError()));
        return;
    }
    QString error;
    if (!m_loop->registerChannel(m_channel, this, &error)) {
        ssh_channel_free(m_channel);
        m_channel = nullptr;
        finishFail(error.isEmpty() ? trMeta("Failed to register channel") : error);
        return;
    }

    m_phase = Phase::RunningCmd;
    m_execState = ExecState::Opening;
    m_timer.start();
    m_loop->wake();
}

void ScpMetaIoHandler::advanceOpening()
{
    if (m_channel == nullptr || m_execState != ExecState::Opening) {
        return;
    }
    const int rc = ssh_channel_open_session(m_channel);
    if (rc == SSH_AGAIN) {
        return;
    }
    if (rc != SSH_OK) {
        finishFail(trMeta("Failed to open channel: %1").arg(sessionError()));
        return;
    }
    m_execState = ExecState::Executing;
    advanceExecuting();
}

void ScpMetaIoHandler::advanceExecuting()
{
    if (m_channel == nullptr || m_execState != ExecState::Executing) {
        return;
    }
    const QByteArray cmd = wrap(m_current.command).toUtf8();
    const int rc = ssh_channel_request_exec(m_channel, cmd.constData());
    if (rc == SSH_AGAIN) {
        return;
    }
    if (rc != SSH_OK) {
        finishFail(trMeta("Failed to execute remote command: %1").arg(sessionError()));
        return;
    }
    m_execState = ExecState::Reading;
}

void ScpMetaIoHandler::tryFinishCmd()
{
    if (m_execState != ExecState::Reading) {
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
            return;
        }
    }

    const QByteArray out = m_stdout;
    const QByteArray err = m_stderr;
    const int exitStatus = m_exitStatus;
    cleanupChannel();
    onCmdFinished(exitStatus, out, err, {});
}

void ScpMetaIoHandler::onCmdFinished(int exitStatus,
                                     const QByteArray &stdoutBytes,
                                     const QByteArray &stderrBytes,
                                     const QString &errorMessage)
{
    if (m_finished || m_cancelled) {
        return;
    }
    if (!errorMessage.isEmpty()) {
        finishFail(errorMessage);
        return;
    }

    ScpEngine *scp = m_fs->scpEngine();
    ShellCommandSet &cmds = scp->commandSet();
    const bool okExit = (exitStatus == 0) || (m_current.allowExitOneWithStdout && exitStatus == 1 &&
                                              !stdoutBytes.isEmpty());

    switch (m_current.kind) {
    case CmdStep::Kind::ShortOk: {
        if (!okExit) {
            finishFail(detailFromResult(exitStatus, stdoutBytes, stderrBytes));
            return;
        }
        if (m_hooks.finished) {
            switch (m_request.op) {
            case Op::CreateDirectory:
                m_hooks.finished(trMeta("Created folder: %1").arg(m_request.path));
                break;
            case Op::CreateSymlink:
                m_hooks.finished(trMeta("Created symlink: %1").arg(m_request.linkPath));
                break;
            case Op::RenamePath:
                m_hooks.finished(trMeta("Renamed to %1").arg(QFileInfo(m_request.to).fileName()));
                break;
            case Op::RemovePath:
                m_hooks.finished(trMeta("Deleted: %1").arg(m_request.path));
                break;
            default:
                break;
            }
        }
        finishOk();
        return;
    }
    case CmdStep::Kind::CanonicalPrimary: {
        if (okExit) {
            m_canonical = QString::fromUtf8(stdoutBytes).trimmed();
            if (!m_canonical.isEmpty()) {
                if (m_hooks.canonicalized) {
                    m_hooks.canonicalized(m_request.path, m_canonical);
                }
                finishOk();
                return;
            }
        }
        const QString fallback =
            QStringLiteral("cd %1 && pwd").arg(ShellCommandSet::shellQuote(m_request.path));
        m_steps.push_front(CmdStep{fallback, false, CmdStep::Kind::CanonicalFallback, {}});
        startNextCmd();
        return;
    }
    case CmdStep::Kind::CanonicalFallback: {
        if (!okExit) {
            finishFail(detailFromResult(exitStatus, stdoutBytes, stderrBytes));
            return;
        }
        m_canonical = QString::fromUtf8(stdoutBytes).trimmed();
        if (m_canonical.isEmpty()) {
            finishFail(trMeta("Cannot resolve remote path"));
            return;
        }
        if (m_hooks.canonicalized) {
            m_hooks.canonicalized(m_request.path, m_canonical);
        }
        finishOk();
        return;
    }
    case CmdStep::Kind::ListProbeFullTime: {
        const QString listPath = Symlink::directoryListPath(m_request.path);
        if (exitStatus == 0) {
            scp->setFullTimeProbed(true, true);
            QString error;
            if (!ShellCommandSet::parseLsListing(
                    QString::fromUtf8(stdoutBytes), &m_listEntries, m_request.path, &error)) {
                finishFail(error);
                return;
            }
            m_annotateIndex = 0;
            for (const RemoteEntry &entry : m_listEntries) {
                if (entry.isSymlink) {
                    m_steps.push_back(CmdStep{cmds.formatTestDirectory(entry.path),
                                              false,
                                              CmdStep::Kind::AnnotateSymlink,
                                              entry.path});
                }
            }
            if (m_steps.empty()) {
                sortEntries(&m_listEntries);
                if (m_resolveNeedsList) {
                    if (m_hooks.resolved) {
                        m_hooks.resolved(m_request.path, true, true, {});
                    }
                } else if (m_hooks.listed) {
                    m_hooks.listed(m_request.path, m_listEntries);
                }
                finishOk();
                return;
            }
            startNextCmd();
            return;
        }
        scp->setFullTimeProbed(true, false);
        m_steps.push_front(CmdStep{cmds.formatListDirectory(listPath, scp->lsOptions()),
                                   m_allowWarn,
                                   CmdStep::Kind::ListMain,
                                   {}});
        startNextCmd();
        return;
    }
    case CmdStep::Kind::ListMain: {
        if (!okExit) {
            const QString error = detailFromResult(exitStatus, stdoutBytes, stderrBytes);
            if (m_resolveNeedsList) {
                if (m_hooks.resolved) {
                    m_hooks.resolved(m_request.path, true, false, error);
                }
                finishOk();
                return;
            }
            finishFail(error);
            return;
        }
        QString error;
        if (!ShellCommandSet::parseLsListing(
                QString::fromUtf8(stdoutBytes), &m_listEntries, m_request.path, &error)) {
            if (m_resolveNeedsList) {
                if (m_hooks.resolved) {
                    m_hooks.resolved(m_request.path, true, false, error);
                }
                finishOk();
                return;
            }
            finishFail(error);
            return;
        }
        m_annotateIndex = 0;
        for (const RemoteEntry &entry : m_listEntries) {
            if (entry.isSymlink) {
                m_steps.push_back(CmdStep{cmds.formatTestDirectory(entry.path),
                                          false,
                                          CmdStep::Kind::AnnotateSymlink,
                                          entry.path});
            }
        }
        if (m_steps.empty()) {
            sortEntries(&m_listEntries);
            if (m_resolveNeedsList) {
                if (m_hooks.resolved) {
                    m_hooks.resolved(m_request.path, true, true, {});
                }
            } else if (m_hooks.listed) {
                m_hooks.listed(m_request.path, m_listEntries);
            }
            finishOk();
            return;
        }
        startNextCmd();
        return;
    }
    case CmdStep::Kind::AnnotateSymlink: {
        const bool isDir = (exitStatus == 0);
        for (RemoteEntry &entry : m_listEntries) {
            if (entry.path == m_current.annotatePath) {
                entry.linkIsDir = isDir;
                break;
            }
        }
        if (m_steps.empty()) {
            sortEntries(&m_listEntries);
            if (m_resolveNeedsList) {
                if (m_hooks.resolved) {
                    m_hooks.resolved(m_request.path, true, true, {});
                }
            } else if (m_hooks.listed) {
                m_hooks.listed(m_request.path, m_listEntries);
            }
            finishOk();
            return;
        }
        startNextCmd();
        return;
    }
    case CmdStep::Kind::ResolveTestDir: {
        m_resolveIsDir = (exitStatus == 0);
        if (m_resolveIsDir) {
            m_resolveNeedsList = true;
            m_request.op = Op::ListDirectory;
            m_listEntries.clear();
            planSteps();
            return;
        }
        m_steps.push_back(CmdStep{cmds.formatListFile(m_request.path, scp->lsOptions()),
                                  m_allowWarn,
                                  CmdStep::Kind::ResolveListFile,
                                  {}});
        startNextCmd();
        return;
    }
    case CmdStep::Kind::ResolveListFile: {
        if (!okExit) {
            if (m_hooks.resolved) {
                m_hooks.resolved(m_request.path,
                                 false,
                                 false,
                                 detailFromResult(exitStatus, stdoutBytes, stderrBytes));
            }
            finishOk();
            return;
        }
        if (m_hooks.resolved) {
            m_hooks.resolved(m_request.path, false, true, {});
        }
        finishOk();
        return;
    }
    case CmdStep::Kind::ResolveListOnly:
        break;
    }
}

void ScpMetaIoHandler::onIdle()
{
    if (!m_started || m_cancelled || m_finished) {
        return;
    }

    if (m_phase == Phase::Plan) {
        planSteps();
        return;
    }

    if (m_phase != Phase::RunningCmd) {
        return;
    }

    if (m_timer.isValid() && m_timer.elapsed() >= kTimeoutMs) {
        finishFail(trMeta("Remote command timed out"));
        return;
    }

    switch (m_execState) {
    case ExecState::Opening:
        advanceOpening();
        break;
    case ExecState::Executing:
        advanceExecuting();
        break;
    case ExecState::Reading:
        tryFinishCmd();
        break;
    case ExecState::Idle:
        break;
    }

    if (!m_finished && m_loop != nullptr &&
        (m_execState == ExecState::Opening || m_execState == ExecState::Executing)) {
        m_loop->wake();
    }
}

int ScpMetaIoHandler::onData(ssh_session session,
                             ssh_channel channel,
                             void *data,
                             uint32_t len, // NOLINT(bugprone-easily-swappable-parameters)
                             int isStderr) // NOLINT(bugprone-easily-swappable-parameters)
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

void ScpMetaIoHandler::onEof(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_eof = true;
}

void ScpMetaIoHandler::onClose(ssh_session session, ssh_channel channel)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_closed = true;
    m_eof = true;
}

void ScpMetaIoHandler::onExitStatus(ssh_session session, ssh_channel channel, int exitStatus)
{
    Q_UNUSED(session);
    Q_UNUSED(channel);
    m_exitStatus = exitStatus;
    m_exitSeen = true;
}
