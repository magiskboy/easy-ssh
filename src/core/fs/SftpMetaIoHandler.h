/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/fs/FsRemote.h"
#include "core/fs/SftpEngine.h"
#include "core/fs/SftpTypes.h"
#include "core/ssh/SshIoHandler.h"

#include <QString>
#include <QUuid>
#include <QVector>

#include <functional>
#include <memory>

class SshIoLoop;

/**
 * One SFTP metadata op on SshIoLoop (Phase 3).
 * Short CRUD ops run in a single onIdle; listDirectory is chunked across ticks.
 */
class SftpMetaIoHandler final : public SshIoHandler
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
        std::function<void()> completed; // always called once when handler finishes
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

    SftpMetaIoHandler(FsRemote *fs, Request request, Hooks hooks);
    ~SftpMetaIoHandler() override;

    QString id() const override;
    void setCompletedHook(std::function<void()> completed);
    bool start(SshIoLoop *loop, QString *error) override;
    void cancel() override;
    void onIdle() override;

private:
    void finishOk();
    void finishFail(const QString &message);
    void runShortOp();
    void tickList();

    QString m_id;
    FsRemote *m_fs = nullptr;
    Request m_request;
    Hooks m_hooks;
    SshIoLoop *m_loop = nullptr;
    bool m_started = false;
    bool m_cancelled = false;
    bool m_finished = false;

    std::unique_ptr<SftpEngine::DirListSession> m_listSession;
    QVector<RemoteEntry> m_listEntries;
    bool m_listOpened = false;
    bool m_resolveNeedsList = false;
    bool m_resolveIsDir = false;
};
