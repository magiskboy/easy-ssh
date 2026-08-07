/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/fs/FsRemote.h"
#include "core/fs/SftpTypes.h"
#include "core/ssh/SshIoHandler.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QUuid>
#include <QVector>

#include <deque>
#include <functional>

#include <libssh/libssh.h>

class SshIoLoop;

/**
 * SCP/shell metadata ops on SshIoLoop (Phase 5).
 * Each shell command runs via nonblocking open_session/request_exec + channel callbacks
 * (same model as ExecIoHandler). Parses with ShellCommandSet.
 */
class ScpMetaIoHandler final : public SshIoHandler, public SshChannelCallbacks
{
public:
    enum class Op : uint8_t
    {
        ListDirectory,
        CreateDirectory,
        CreateSymlink,
        RenamePath,
        RemovePath,
        ResolveEntry,
        CanonicalizePath,
    };

    struct Hooks
    {
        std::function<void(const QString &path, const QVector<RemoteEntry> &entries)> listed;
        std::function<void(const QString &path, bool isDir, bool ok, const QString &error)>
            resolved;
        std::function<void(const QString &requested, const QString &canonical)> canonicalized;
        std::function<void(const QString &message)> finished;
        std::function<void(const QString &message)> failed;
        std::function<void()> completed;
    };

    struct Request
    {
        Op op = Op::ListDirectory;
        QString path;
        QString from;
        QString to;
        QString target;
        QString linkPath;
        bool recursive = false;
    };

    ScpMetaIoHandler(FsRemote *fs, Request request, Hooks hooks);
    ~ScpMetaIoHandler() override;

    QString id() const override;
    void setCompletedHook(std::function<void()> completed);
    bool start(SshIoLoop *loop, QString *error) override;
    void cancel() override;
    void onIdle() override;

    int onData(
        ssh_session session, ssh_channel channel, void *data, uint32_t len, int isStderr) override;
    void onEof(ssh_session session, ssh_channel channel) override;
    void onClose(ssh_session session, ssh_channel channel) override;
    void onExitStatus(ssh_session session, ssh_channel channel, int exitStatus) override;

private:
    enum class ExecState : uint8_t
    {
        Idle,
        Opening,
        Executing,
        Reading,
    };

    enum class Phase : uint8_t
    {
        Plan,
        RunningCmd,
        Done,
    };

    struct CmdStep
    {
        QString command;
        bool allowExitOneWithStdout = false;
        enum class Kind : uint8_t
        {
            ListProbeFullTime,
            ListMain,
            AnnotateSymlink,
            ShortOk,
            CanonicalPrimary,
            CanonicalFallback,
            ResolveTestDir,
            ResolveListFile,
            ResolveListOnly,
        } kind = Kind::ShortOk;
        QString annotatePath; // for AnnotateSymlink
    };

    void planSteps();
    void startNextCmd();
    void advanceOpening();
    void advanceExecuting();
    void tryFinishCmd();
    void onCmdFinished(int exitStatus,
                       const QByteArray &stdoutBytes,
                       const QByteArray &stderrBytes,
                       const QString &errorMessage);
    void cleanupChannel();
    void finishOk();
    void finishFail(const QString &message);
    QString sessionError() const;
    QString wrap(const QString &command) const;

    QString m_id;
    FsRemote *m_fs = nullptr;
    Request m_request;
    Hooks m_hooks;
    SshIoLoop *m_loop = nullptr;
    ssh_session m_session = nullptr;

    Phase m_phase = Phase::Plan;
    std::deque<CmdStep> m_steps;
    CmdStep m_current;
    bool m_started = false;
    bool m_cancelled = false;
    bool m_finished = false;

    ExecState m_execState = ExecState::Idle;
    ssh_channel m_channel = nullptr;
    QByteArray m_stdout;
    QByteArray m_stderr;
    int m_exitStatus = -1;
    bool m_exitSeen = false;
    bool m_eof = false;
    bool m_closed = false;
    QElapsedTimer m_timer;

    QVector<RemoteEntry> m_listEntries;
    int m_annotateIndex = 0;
    bool m_resolveNeedsList = false;
    bool m_resolveIsDir = false;
    QString m_canonical;
    bool m_allowWarn = false;
};
