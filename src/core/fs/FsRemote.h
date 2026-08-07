/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "FsEngine.h"
#include "ScpChunkTransfer.h"
#include "SftpAioTransfer.h"
#include "SftpTypes.h"
#include "TransferTypes.h"
#include "core/connection/Connection.h"
#include "core/session/SessionTypes.h"

#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <optional>

#include <QScopeGuard>

#include <libssh/libssh.h>

class SftpEngine;

/**
 * High-level remote FS entity: recursive transfer, progress, cancel, resume jobs.
 * Prefers SftpEngine; falls back to ScpEngine + shell CommandSet when configured.
 */
class FsRemote
{
public:
    using ProgressCallback =
        std::function<void(qint64 bytesDone, qint64 bytesTotal, const QString &currentName)>;

    enum class TickResult : uint8_t
    {
        Idle = 0,
        Running = 1,
        Done = 2,
        Failed = 3,
    };

    FsRemote() = default;
    ~FsRemote();

    FsRemote(const FsRemote &) = delete;
    FsRemote &operator=(const FsRemote &) = delete;

    void setEngine(std::unique_ptr<FsEngine> engine);
    FsEngine *engine() const { return m_engine.get(); }
    SftpEngine *sftpEngine() const;
    class ScpEngine *scpEngine() const;
    ssh_session sshSession() const { return m_sshSession; }

    void setShellCommands(const ShellCommandSetConfig &config);
    ShellCommandSetConfig shellCommands() const { return m_shellCommands; }

    void setConnectionId(const QUuid &connectionId) { m_connectionId = connectionId; }
    QUuid connectionId() const { return m_connectionId; }

    /// Stall timeout in seconds; 0 disables. Checked during progress/cancel polls.
    void setStallTimeoutSec(int seconds) { m_stallTimeoutSec = qMax(0, seconds); }
    int stallTimeoutSec() const { return m_stallTimeoutSec; }

    /// Try SFTP, then SCP+shell when allowScpFallback. Sets backend on success.
    bool open(ssh_session session, QString *failureMessage = nullptr);
    void close();
    bool isOpen() const;
    FsBackend backend() const { return m_backend; }

    void requestCancel();
    void requestInterrupt(TransferEndReason reason, const QString &message = {});
    void setProgressCallback(ProgressCallback callback);

    bool wasCanceled() const;
    bool wasInterrupted() const;
    TransferEndReason lastEndReason() const { return m_lastEndReason; }
    QString lastEndMessage() const { return m_lastEndMessage; }
    std::optional<TransferJob> lastInterruptedJob() const { return m_lastInterruptedJob; }

    bool
    listDirectoryEntries(const QString &path, QVector<RemoteEntry> *outEntries, QString *error);
    bool createDirectory(const QString &path, QString *error);
    bool createSymlink(const QString &target, const QString &linkPath, QString *error);
    bool renamePath(const QString &from, const QString &to, QString *error);
    bool removePath(const QString &path, bool recursive, QString *error);
    bool canonicalizePath(const QString &path, QString &canonicalOut, QString *error);
    /// Follow symlinks: report whether the resolved target is a directory.
    bool resolveEntry(const QString &path, bool *isDir, QString *error);

    bool uploadFiles(const QStringList &localPaths, const QString &remoteDir, QString *error);
    bool uploadFileTo(const QString &localPath, const QString &remotePath, QString *error);
    /// @p followSymlinks true: download symlink target content as a regular file (Open With).
    /// false (default): preserve symlinks as local symlinks.
    bool downloadPaths(const QStringList &remotePaths,
                       const QString &localDir,
                       QString *error,
                       bool followSymlinks = false);

    /// Resume the persisted interrupted job for this connection (SFTP only).
    bool resumeInterruptedTransfer(QString *error);
    bool discardInterruptedTransfer(QString *error);

    /// Phase 3/5: non-blocking transfer driven from SshIoLoop::onIdle (SFTP AIO or SCP chunks).
    /// Returns false if start fails (error set).
    bool
    beginAsyncUploadFiles(const QStringList &localPaths, const QString &remoteDir, QString *error);
    bool
    beginAsyncUploadFileTo(const QString &localPath, const QString &remotePath, QString *error);
    bool beginAsyncDownloadPaths(const QStringList &remotePaths,
                                 const QString &localDir,
                                 QString *error,
                                 bool followSymlinks = false);
    bool beginAsyncResumeInterrupted(QString *error);
    TickResult tickAsync(QString *error);
    bool hasAsyncTransfer() const { return m_asyncActive; }
    void abortAsyncTransfer();

    /// Brief blocking helpers for meta IoHandler (caller owns session blocking policy).
    template <typename Fn>
    auto withBlockingSession(Fn &&fn) -> decltype(fn())
    {
        if (m_sshSession == nullptr) {
            return fn();
        }
        const int wasBlocking = ssh_is_blocking(m_sshSession);
        ssh_set_blocking(m_sshSession, 1);
        auto restore = qScopeGuard([this, wasBlocking]() {
            if (m_sshSession != nullptr) {
                ssh_set_blocking(m_sshSession, wasBlocking);
            }
        });
        return fn();
    }

private:
    struct AsyncWorkItem
    {
        enum class Type : uint8_t
        {
            UploadFile,
            DownloadFile,
            UploadDir,
            DownloadDir,
            RemoteMkdir,
            LocalMkdir,
            RemoteSymlink,
            LocalSymlink,
        };
        Type type = Type::UploadFile;
        QString localPath;
        QString remotePath;
        QString symlinkTarget;
        TransferWriteMode mode = TransferWriteMode::FreshFilepart;
        bool followSymlinks = false;
        bool linkIsDir = false;
    };

    void beginTransfer(qint64 bytesTotal);
    void endTransfer();
    bool transferShouldStop(QString *error) const;
    void noteTransferProgress(qint64 bytesDelta, const QString &currentName);
    void seedProgressBytes(qint64 bytesDone);
    void touchProgressActivity();

    qint64 computeLocalBytes(const QStringList &localPaths) const;
    qint64 computeLocalPathBytes(const QString &localPath) const;
    qint64 computeRemoteBytes(const QStringList &remotePaths);
    qint64 computeRemotePathBytes(const RemoteEntry &entry);

    bool removePathRecursive(const QString &path, QString *error);
    bool uploadPathRecursive(const QString &localPath,
                             const QString &remotePath,
                             TransferWriteMode mode,
                             QString *error);
    bool downloadPathRecursive(const RemoteEntry &entry,
                               const QString &localPath,
                               TransferWriteMode mode,
                               QString *error,
                               bool followSymlinks);

    bool transferOneUpload(const QString &localPath,
                           const QString &remoteFinalPath,
                           TransferWriteMode mode,
                           QString *error);
    bool transferOneDownload(const QString &remoteFinalPath,
                             const QString &localFinalPath,
                             TransferWriteMode mode,
                             QString *error);

    void persistInterruptedJob(const TransferJob &job);
    TransferOptions optionsForMode(TransferWriteMode mode, const TransferJob *resumeJob) const;

    static QString joinRemotePath(const QString &dir, const QString &name);

    bool startAsyncCommon(qint64 bytesTotal, QString *error);
    void
    enqueueUploadPath(const QString &localPath, const QString &remotePath, TransferWriteMode mode);
    bool enqueueDownloadEntry(const RemoteEntry &entry,
                              const QString &localPath,
                              TransferWriteMode mode,
                              bool followSymlinks,
                              QString *error);
    TickResult tickAsyncWorkItem(QString *error);
    TickResult tickAsyncAio(QString *error);
    bool finishAsyncFileFailure(QString *error);
    bool prepareAsyncUploadJob(const QString &localPath,
                               const QString &remoteFinalPath,
                               TransferWriteMode mode,
                               QString *error);
    bool prepareAsyncDownloadJob(const QString &remoteFinalPath,
                                 const QString &localFinalPath,
                                 TransferWriteMode mode,
                                 QString *error);

    ssh_session m_sshSession = nullptr;
    std::unique_ptr<FsEngine> m_engine;
    ShellCommandSetConfig m_shellCommands;
    FsBackend m_backend = FsBackend::None;
    QUuid m_connectionId;
    ProgressCallback m_progressCallback;
    std::atomic_bool m_transferCancel{false};
    std::atomic_bool m_transferInterrupt{false};
    TransferEndReason m_interruptReason = TransferEndReason::Interrupted;
    QString m_interruptMessage;
    TransferEndReason m_lastEndReason = TransferEndReason::Completed;
    QString m_lastEndMessage;
    std::optional<TransferJob> m_lastInterruptedJob;
    std::optional<TransferJob> m_activeJob;
    qint64 m_progressBytesDone = 0;
    qint64 m_progressBytesTotal = -1;
    qint64 m_progressLastEmitBytes = 0;
    qint64 m_progressLastEmitMs = 0;
    qint64 m_progressLastActivityMs = 0;
    int m_stallTimeoutSec = 60;

    bool m_asyncActive = false;
    std::deque<AsyncWorkItem> m_asyncWork;
    std::unique_ptr<SftpAioTransfer> m_aio;
    std::unique_ptr<ScpChunkTransfer> m_scpChunk;
    TransferWriteMode m_asyncFileMode = TransferWriteMode::FreshFilepart;
};
