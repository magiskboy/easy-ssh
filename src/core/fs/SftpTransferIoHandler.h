/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/fs/FsRemote.h"
#include "core/ssh/SshIoHandler.h"

#include <QString>
#include <QStringList>
#include <QUuid>

#include <functional>

class SshIoLoop;

/**
 * One SFTP transfer job on SshIoLoop (Phase 3).
 * Drives FsRemote::tickAsync from onIdle; progress/cancel via existing FsRemote atomics.
 */
class SftpTransferIoHandler final : public SshIoHandler
{
public:
    enum class Kind : uint8_t
    {
        UploadFiles,
        UploadFileTo,
        DownloadPaths,
        ResumeInterrupted,
    };

    struct Hooks
    {
        std::function<void(const QString &message)> finished;
        std::function<void(const QString &error)> failed;
        std::function<void()> completed;
    };

    struct Request
    {
        Kind kind = Kind::UploadFiles;
        QStringList localPaths;
        QStringList remotePaths;
        QString remoteDir;
        QString localDir;
        QString localPath;
        QString remotePath;
        bool followSymlinks = false;
        QString finishedMessage;
    };

    SftpTransferIoHandler(FsRemote *fs, Request request, Hooks hooks);
    ~SftpTransferIoHandler() override;

    QString id() const override;
    void setCompletedHook(std::function<void()> completed);
    bool start(SshIoLoop *loop, QString *error) override;
    void cancel() override;
    void onIdle() override;

private:
    void finishOk();
    void finishFail(const QString &error);

    QString m_id;
    FsRemote *m_fs = nullptr;
    Request m_request;
    Hooks m_hooks;
    SshIoLoop *m_loop = nullptr;
    bool m_started = false;
    bool m_cancelled = false;
    bool m_finished = false;
    bool m_begun = false;
};
