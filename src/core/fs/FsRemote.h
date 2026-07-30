/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "FsEngine.h"
#include "SftpTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

#include <libssh/libssh.h>

/**
 * High-level remote FS entity: recursive transfer, progress, cancel.
 * Depends on FsEngine (SftpEngine today; ScpEngine later).
 */
class FsRemote
{
public:
    using ProgressCallback =
        std::function<void(qint64 bytesDone, qint64 bytesTotal, const QString &currentName)>;

    FsRemote() = default;
    ~FsRemote();

    FsRemote(const FsRemote &) = delete;
    FsRemote &operator=(const FsRemote &) = delete;

    void setEngine(std::unique_ptr<FsEngine> engine);
    FsEngine *engine() const { return m_engine.get(); }

    bool open(ssh_session session, QString *failureMessage = nullptr);
    void close();
    bool isOpen() const;

    void requestCancel();
    void setProgressCallback(ProgressCallback callback);
    bool wasCanceled() const;

    bool
    listDirectoryEntries(const QString &path, QVector<RemoteEntry> *outEntries, QString *error);
    bool createDirectory(const QString &path, QString *error);
    bool renamePath(const QString &from, const QString &to, QString *error);
    bool removePath(const QString &path, bool recursive, QString *error);
    bool canonicalizePath(const QString &path, QString *canonicalOut, QString *error);

    bool uploadFiles(const QStringList &localPaths, const QString &remoteDir, QString *error);
    bool uploadFileTo(const QString &localPath, const QString &remotePath, QString *error);
    bool downloadPaths(const QStringList &remotePaths, const QString &localDir, QString *error);

private:
    void beginTransfer(qint64 bytesTotal);
    void endTransfer();
    bool transferCanceled(QString *error) const;
    void noteTransferProgress(qint64 bytesDelta, const QString &currentName);

    qint64 computeLocalBytes(const QStringList &localPaths) const;
    qint64 computeLocalPathBytes(const QString &localPath) const;
    qint64 computeRemoteBytes(const QStringList &remotePaths);
    qint64 computeRemotePathBytes(const QString &remotePath, bool isDir);

    bool removePathRecursive(const QString &path, QString *error);
    bool uploadPathRecursive(const QString &localPath, const QString &remotePath, QString *error);
    bool downloadPathRecursive(const QString &remotePath,
                               const QString &localPath,
                               bool isDir,
                               QString *error);

    static QString joinRemotePath(const QString &dir, const QString &name);

    std::unique_ptr<FsEngine> m_engine;
    ProgressCallback m_progressCallback;
    std::atomic_bool m_transferCancel{false};
    qint64 m_progressBytesDone = 0;
    qint64 m_progressBytesTotal = -1;
    qint64 m_progressLastEmitBytes = 0;
    qint64 m_progressLastEmitMs = 0;
};
