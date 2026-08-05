// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "FsRemote.h"

#include "ScpEngine.h"
#include "SftpEngine.h"
#include "Symlink.h"
#include "TransferJobStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <sys/stat.h>

#ifndef S_IRWXU
#define S_IRUSR 00400u
#define S_IWUSR 00200u
#define S_IXUSR 00100u
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif

namespace
{
constexpr qint64 kProgressEmitBytes = 64 * 1024;
constexpr qint64 kProgressEmitMs = 100;

QString trFs(const char *text)
{
    return QCoreApplication::translate("FsRemote", text);
}
} // namespace

FsRemote::~FsRemote()
{
    close();
}

void FsRemote::setEngine(std::unique_ptr<FsEngine> engine)
{
    close();
    m_engine = std::move(engine);
    m_backend = FsBackend::None;
}

void FsRemote::setShellCommands(const ShellCommandSetConfig &config)
{
    m_shellCommands = config;
}

bool FsRemote::open(ssh_session session, QString *failureMessage)
{
    close();

    auto sftp = std::make_unique<SftpEngine>();
    QString sftpError;
    if (sftp->open(session, &sftpError)) {
        m_engine = std::move(sftp);
        m_backend = FsBackend::Sftp;
        return true;
    }

    if (!m_shellCommands.allowScpFallback) {
        if (failureMessage) {
            *failureMessage = sftpError;
        }
        return false;
    }

    auto scp = std::make_unique<ScpEngine>(m_shellCommands);
    QString scpError;
    if (scp->open(session, &scpError)) {
        m_engine = std::move(scp);
        m_backend = FsBackend::Scp;
        return true;
    }

    if (failureMessage) {
        *failureMessage =
            trFs("SFTP unavailable (%1); SCP fallback failed (%2)").arg(sftpError, scpError);
    }
    return false;
}

void FsRemote::close()
{
    if (m_engine) {
        m_engine->close();
    }
    m_backend = FsBackend::None;
    endTransfer();
}

bool FsRemote::isOpen() const
{
    return m_engine && m_engine->isOpen();
}

void FsRemote::setProgressCallback(ProgressCallback callback)
{
    m_progressCallback = std::move(callback);
}

void FsRemote::requestCancel()
{
    m_transferCancel.store(true, std::memory_order_relaxed);
}

void FsRemote::requestInterrupt(TransferEndReason reason, const QString &message)
{
    m_interruptReason = reason;
    m_interruptMessage = message;
    m_transferInterrupt.store(true, std::memory_order_relaxed);
}

bool FsRemote::wasCanceled() const
{
    return m_lastEndReason == TransferEndReason::Canceled;
}

bool FsRemote::wasInterrupted() const
{
    return m_lastEndReason == TransferEndReason::Interrupted ||
           m_lastEndReason == TransferEndReason::StallTimeout;
}

void FsRemote::beginTransfer(qint64 bytesTotal)
{
    m_transferCancel.store(false, std::memory_order_relaxed);
    m_transferInterrupt.store(false, std::memory_order_relaxed);
    m_interruptReason = TransferEndReason::Interrupted;
    m_interruptMessage.clear();
    m_lastEndReason = TransferEndReason::Completed;
    m_lastEndMessage.clear();
    m_lastInterruptedJob.reset();
    m_activeJob.reset();
    m_progressBytesDone = 0;
    m_progressBytesTotal = bytesTotal;
    m_progressLastEmitBytes = 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_progressLastEmitMs = now;
    m_progressLastActivityMs = now;
    if (m_progressCallback) {
        m_progressCallback(0, m_progressBytesTotal, QString());
    }
}

void FsRemote::endTransfer()
{
    // Preserve cancel/interrupt flags until the caller reads wasCanceled/wasInterrupted.
    m_activeJob.reset();
}

void FsRemote::seedProgressBytes(qint64 bytesDone)
{
    if (bytesDone > m_progressBytesDone) {
        m_progressBytesDone = bytesDone;
    }
    touchProgressActivity();
    if (m_progressCallback) {
        m_progressCallback(m_progressBytesDone, m_progressBytesTotal, QString());
    }
}

void FsRemote::touchProgressActivity()
{
    m_progressLastActivityMs = QDateTime::currentMSecsSinceEpoch();
}

bool FsRemote::transferShouldStop(QString *error) const
{
    if (m_transferCancel.load(std::memory_order_relaxed)) {
        if (error) {
            *error = trFs("Transfer canceled");
        }
        return true;
    }
    if (m_transferInterrupt.load(std::memory_order_relaxed)) {
        if (error) {
            *error =
                m_interruptMessage.isEmpty() ? trFs("Transfer interrupted") : m_interruptMessage;
        }
        return true;
    }
    if (m_stallTimeoutSec > 0 && m_progressLastActivityMs > 0) {
        const qint64 idleMs = QDateTime::currentMSecsSinceEpoch() - m_progressLastActivityMs;
        if (idleMs >= static_cast<qint64>(m_stallTimeoutSec) * 1000) {
            if (error) {
                *error =
                    trFs("Transfer stalled (no progress for %1 seconds)").arg(m_stallTimeoutSec);
            }
            return true;
        }
    }
    return false;
}

void FsRemote::noteTransferProgress(qint64 bytesDelta, const QString &currentName)
{
    if (bytesDelta > 0) {
        m_progressBytesDone += bytesDelta;
        touchProgressActivity();
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool bytesThreshold =
        (m_progressBytesDone - m_progressLastEmitBytes) >= kProgressEmitBytes;
    const bool timeThreshold = (now - m_progressLastEmitMs) >= kProgressEmitMs;
    if (!bytesThreshold && !timeThreshold && bytesDelta > 0) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        return;
    }

    m_progressLastEmitBytes = m_progressBytesDone;
    m_progressLastEmitMs = now;
    if (m_progressCallback) {
        m_progressCallback(m_progressBytesDone, m_progressBytesTotal, currentName);
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

QString FsRemote::joinRemotePath(const QString &dir, const QString &name)
{
    if (dir.isEmpty() || dir == QLatin1String(".")) {
        return name;
    }
    if (dir.endsWith(QLatin1Char('/'))) {
        return dir + name;
    }
    return dir + QLatin1Char('/') + name;
}

TransferOptions FsRemote::optionsForMode(TransferWriteMode mode, const TransferJob *resumeJob) const
{
    TransferOptions opts;
    opts.mode = mode;
    if (mode == TransferWriteMode::ResumeFilepart && resumeJob) {
        opts.resumeOffset = resumeJob->bytesDone;
        opts.sha256PrefixHex = resumeJob->sha256PrefixHex;
        opts.expectedSha256FullHex = resumeJob->sha256FullHex;
    }
    if (m_backend != FsBackend::Sftp) {
        opts.mode = TransferWriteMode::OverwriteFinal;
    }
    return opts;
}

void FsRemote::persistInterruptedJob(const TransferJob &job)
{
    TransferJob stored = job;
    stored.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    TransferJobStore::save(stored);
    m_lastInterruptedJob = stored;
}

bool FsRemote::listDirectoryEntries(const QString &path,
                                    QVector<RemoteEntry> *outEntries,
                                    QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->listDirectoryEntries(path, outEntries, error);
}

bool FsRemote::createDirectory(const QString &path, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->createDirectory(path, error);
}

bool FsRemote::createSymlink(const QString &target, const QString &linkPath, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->createSymlink(target, linkPath, error);
}

bool FsRemote::renamePath(const QString &from, const QString &to, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->renamePath(from, to, error);
}

bool FsRemote::canonicalizePath(const QString &path, QString &canonicalOut, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->canonicalizePath(path, canonicalOut, error);
}

bool FsRemote::resolveEntry(const QString &path, bool *isDir, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->isRemoteDirectory(path, isDir, error);
}

bool FsRemote::removePath(const QString &path, bool recursive, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    RemoteEntry entry;
    if (!m_engine->statEntry(path, &entry, false, error)) {
        return false;
    }

    if (entry.isSymlink) {
        return m_engine->removeFile(path, error);
    }
    if (entry.isDir) {
        if (recursive) {
            return removePathRecursive(path, error);
        }
        return m_engine->removeDirectory(path, error);
    }
    return m_engine->removeFile(path, error);
}

qint64 FsRemote::computeLocalBytes(const QStringList &localPaths) const
{
    qint64 total = 0;
    for (const QString &path : localPaths) {
        const qint64 part = computeLocalPathBytes(path);
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

qint64 FsRemote::computeLocalPathBytes(const QString &localPath) const
{
    const QFileInfo info(localPath);
    if (info.isSymLink()) {
        return 0;
    }
    if (!info.exists()) {
        return 0;
    }
    if (!info.isDir()) {
        return info.size();
    }

    qint64 total = 0;
    const QDir dir(localPath);
    const auto children =
        dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &child : children) {
        const qint64 part = computeLocalPathBytes(child.absoluteFilePath());
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

qint64 FsRemote::computeRemoteBytes(const QStringList &remotePaths)
{
    qint64 total = 0;
    for (const QString &path : remotePaths) {
        RemoteEntry entry;
        QString error;
        if (!m_engine->statEntry(path, &entry, false, &error)) {
            return -1;
        }
        const qint64 part = computeRemotePathBytes(entry);
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

qint64 FsRemote::computeRemotePathBytes(const RemoteEntry &entry)
{
    if (entry.isSymlink) {
        return 0;
    }
    if (!entry.isDir) {
        qint64 size = 0;
        QString error;
        if (!m_engine->remoteFileSize(entry.path, &size, &error)) {
            return -1;
        }
        return size;
    }

    QVector<RemoteEntry> children;
    QString error;
    if (!m_engine->listDirectoryEntries(entry.path, &children, &error)) {
        return -1;
    }

    qint64 total = 0;
    for (const RemoteEntry &child : children) {
        if (isTransferFilepartName(child.name)) {
            continue;
        }
        const qint64 part = computeRemotePathBytes(child);
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

bool FsRemote::transferOneUpload(const QString &localPath,
                                 const QString &remoteFinalPath,
                                 TransferWriteMode mode,
                                 QString *error)
{
    const QFileInfo info(localPath);
    TransferJob job;
    job.connectionId = m_connectionId;
    job.direction = TransferDirection::Upload;
    job.localPath = localPath;
    job.remoteFinalPath = remoteFinalPath;
    job.filepartPath = transferFilepartPathForFinal(remoteFinalPath);
    job.bytesTotal = info.size();
    job.sourceSize = info.size();
    job.sourceMtimeUtcMs = info.lastModified().toUTC().toMSecsSinceEpoch();
    job.backend = m_backend;

    const TransferJob *resumePtr = nullptr;
    TransferJob resumeJob;
    if (mode == TransferWriteMode::ResumeFilepart) {
        auto loaded = TransferJobStore::loadForPaths(
            m_connectionId, TransferDirection::Upload, localPath, remoteFinalPath);
        if (!loaded) {
            if (error) {
                *error = trFs("No resumable upload job found");
            }
            return false;
        }
        resumeJob = *loaded;
        resumePtr = &resumeJob;
        job = resumeJob;
        seedProgressBytes(job.bytesDone);
    }

    m_activeJob = job;
    const TransferOptions opts = optionsForMode(mode, resumePtr);

    const auto shouldCancel = [this](QString *err) {
        if (!transferShouldStop(err)) {
            return false;
        }
        if (m_transferCancel.load(std::memory_order_relaxed)) {
            m_lastEndReason = TransferEndReason::Canceled;
        } else if (m_stallTimeoutSec > 0 &&
                   (QDateTime::currentMSecsSinceEpoch() - m_progressLastActivityMs) >=
                       static_cast<qint64>(m_stallTimeoutSec) * 1000) {
            m_lastEndReason = TransferEndReason::StallTimeout;
            m_lastEndMessage = err ? *err : QString();
        } else {
            m_lastEndReason = m_interruptReason;
            m_lastEndMessage = m_interruptMessage;
        }
        return true;
    };
    const auto onProgress = [this](qint64 delta, const QString &name) {
        noteTransferProgress(delta, name);
    };

    qint64 partialBytes = 0;
    QString partialHash;
    const bool ok = m_engine->uploadFile(localPath,
                                         shouldCancel,
                                         remoteFinalPath,
                                         onProgress,
                                         opts,
                                         error,
                                         &partialBytes,
                                         &partialHash);

    if (ok) {
        TransferJobStore::removeJob(job);
        m_lastEndReason = TransferEndReason::Completed;
        m_activeJob.reset();
        return true;
    }

    const bool hashFail =
        error && error->contains(QLatin1String("hash mismatch"), Qt::CaseInsensitive);
    if (hashFail) {
        m_lastEndReason = TransferEndReason::HashMismatch;
        m_lastEndMessage = error ? *error : QString();
        m_activeJob.reset();
        return false;
    }

    if (m_backend == FsBackend::Sftp && mode != TransferWriteMode::OverwriteFinal &&
        partialBytes > 0 && !partialHash.isEmpty()) {
        job.bytesDone = partialBytes;
        job.sha256PrefixHex = partialHash;
        job.lastReason = m_lastEndReason;
        job.lastMessage = error ? *error : QString();
        if (m_lastEndReason == TransferEndReason::Completed) {
            // Engine failed before we classified cancel/interrupt.
            if (m_transferCancel.load(std::memory_order_relaxed)) {
                m_lastEndReason = TransferEndReason::Canceled;
            } else if (m_transferInterrupt.load(std::memory_order_relaxed)) {
                m_lastEndReason = m_interruptReason;
            } else if (m_stallTimeoutSec > 0 &&
                       (QDateTime::currentMSecsSinceEpoch() - m_progressLastActivityMs) >=
                           static_cast<qint64>(m_stallTimeoutSec) * 1000) {
                m_lastEndReason = TransferEndReason::StallTimeout;
            } else {
                m_lastEndReason = TransferEndReason::Interrupted;
            }
            job.lastReason = m_lastEndReason;
        }
        persistInterruptedJob(job);
    } else if (m_lastEndReason == TransferEndReason::Completed) {
        m_lastEndReason = TransferEndReason::Failed;
        m_lastEndMessage = error ? *error : QString();
    }
    m_activeJob.reset();
    return false;
}

bool FsRemote::transferOneDownload(const QString &remoteFinalPath,
                                   const QString &localFinalPath,
                                   TransferWriteMode mode,
                                   QString *error)
{
    qint64 remoteSize = 0;
    if (!m_engine->remoteFileSize(remoteFinalPath, &remoteSize, error)) {
        return false;
    }

    TransferJob job;
    job.connectionId = m_connectionId;
    job.direction = TransferDirection::Download;
    job.localPath = localFinalPath;
    job.remoteFinalPath = remoteFinalPath;
    job.filepartPath = transferFilepartPathForFinal(localFinalPath);
    job.bytesTotal = remoteSize;
    job.sourceSize = remoteSize;
    job.backend = m_backend;

    const TransferJob *resumePtr = nullptr;
    TransferJob resumeJob;
    if (mode == TransferWriteMode::ResumeFilepart) {
        auto loaded = TransferJobStore::loadForPaths(
            m_connectionId, TransferDirection::Download, localFinalPath, remoteFinalPath);
        if (!loaded) {
            if (error) {
                *error = trFs("No resumable download job found");
            }
            return false;
        }
        resumeJob = *loaded;
        resumePtr = &resumeJob;
        job = resumeJob;
        seedProgressBytes(job.bytesDone);
    }

    m_activeJob = job;
    const TransferOptions opts = optionsForMode(mode, resumePtr);

    const auto shouldCancel = [this](QString *err) {
        if (!transferShouldStop(err)) {
            return false;
        }
        if (m_transferCancel.load(std::memory_order_relaxed)) {
            m_lastEndReason = TransferEndReason::Canceled;
        } else if (m_stallTimeoutSec > 0 &&
                   (QDateTime::currentMSecsSinceEpoch() - m_progressLastActivityMs) >=
                       static_cast<qint64>(m_stallTimeoutSec) * 1000) {
            m_lastEndReason = TransferEndReason::StallTimeout;
            m_lastEndMessage = err ? *err : QString();
        } else {
            m_lastEndReason = m_interruptReason;
            m_lastEndMessage = m_interruptMessage;
        }
        return true;
    };
    const auto onProgress = [this](qint64 delta, const QString &name) {
        noteTransferProgress(delta, name);
    };

    qint64 partialBytes = 0;
    QString partialHash;
    const bool ok = m_engine->downloadFile(remoteFinalPath,
                                           shouldCancel,
                                           localFinalPath,
                                           onProgress,
                                           opts,
                                           error,
                                           &partialBytes,
                                           &partialHash);

    if (ok) {
        TransferJobStore::removeJob(job);
        QFile::remove(transferMetaPathForFilepart(job.filepartPath));
        m_lastEndReason = TransferEndReason::Completed;
        m_activeJob.reset();
        return true;
    }

    const bool hashFail =
        error && error->contains(QLatin1String("hash mismatch"), Qt::CaseInsensitive);
    if (hashFail) {
        m_lastEndReason = TransferEndReason::HashMismatch;
        m_lastEndMessage = error ? *error : QString();
        m_activeJob.reset();
        return false;
    }

    if (m_backend == FsBackend::Sftp && mode != TransferWriteMode::OverwriteFinal &&
        partialBytes > 0 && !partialHash.isEmpty()) {
        job.bytesDone = partialBytes;
        job.sha256PrefixHex = partialHash;
        if (m_lastEndReason == TransferEndReason::Completed) {
            if (m_transferCancel.load(std::memory_order_relaxed)) {
                m_lastEndReason = TransferEndReason::Canceled;
            } else if (m_transferInterrupt.load(std::memory_order_relaxed)) {
                m_lastEndReason = m_interruptReason;
            } else if (m_stallTimeoutSec > 0 &&
                       (QDateTime::currentMSecsSinceEpoch() - m_progressLastActivityMs) >=
                           static_cast<qint64>(m_stallTimeoutSec) * 1000) {
                m_lastEndReason = TransferEndReason::StallTimeout;
            } else {
                m_lastEndReason = TransferEndReason::Interrupted;
            }
        }
        job.lastReason = m_lastEndReason;
        job.lastMessage = error ? *error : QString();
        persistInterruptedJob(job);
    } else if (m_lastEndReason == TransferEndReason::Completed) {
        m_lastEndReason = TransferEndReason::Failed;
        m_lastEndMessage = error ? *error : QString();
    }
    m_activeJob.reset();
    return false;
}

bool FsRemote::uploadFiles(const QStringList &localPaths, const QString &remoteDir, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    beginTransfer(computeLocalBytes(localPaths));
    const TransferWriteMode mode = (m_backend == FsBackend::Sftp)
                                       ? TransferWriteMode::FreshFilepart
                                       : TransferWriteMode::OverwriteFinal;

    for (const QString &localPath : localPaths) {
        if (transferShouldStop(error)) {
            if (m_transferCancel.load(std::memory_order_relaxed)) {
                m_lastEndReason = TransferEndReason::Canceled;
            } else if (m_transferInterrupt.load(std::memory_order_relaxed)) {
                m_lastEndReason = m_interruptReason;
                m_lastEndMessage = m_interruptMessage;
            } else {
                m_lastEndReason = TransferEndReason::StallTimeout;
                m_lastEndMessage = error ? *error : QString();
            }
            endTransfer();
            return false;
        }

        const QFileInfo info(localPath);
        if (!info.exists()) {
            endTransfer();
            if (error) {
                *error = trFs("Local path does not exist: %1").arg(localPath);
            }
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }

        const QString remotePath = joinRemotePath(remoteDir, info.fileName());
        if (!uploadPathRecursive(localPath, remotePath, mode, error)) {
            endTransfer();
            return false;
        }
    }

    endTransfer();
    m_lastEndReason = TransferEndReason::Completed;
    return true;
}

bool FsRemote::uploadFileTo(const QString &localPath, const QString &remotePath, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    const QFileInfo info(localPath);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = trFs("Local file does not exist: %1").arg(localPath);
        }
        return false;
    }

    beginTransfer(info.size());
    // Open With / auto-sync: always overwrite final path (no resume).
    const bool ok =
        transferOneUpload(localPath, remotePath, TransferWriteMode::OverwriteFinal, error);
    endTransfer();
    return ok;
}

bool FsRemote::downloadPaths(const QStringList &remotePaths,
                             const QString &localDir,
                             QString *error,
                             bool followSymlinks)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    QDir local(localDir);
    if (!local.exists() && !local.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = trFs("Cannot create local directory: %1").arg(localDir);
        }
        return false;
    }

    beginTransfer(computeRemoteBytes(remotePaths));
    const TransferWriteMode mode = (m_backend == FsBackend::Sftp)
                                       ? TransferWriteMode::FreshFilepart
                                       : TransferWriteMode::OverwriteFinal;

    for (const QString &remotePath : remotePaths) {
        if (transferShouldStop(error)) {
            if (m_transferCancel.load(std::memory_order_relaxed)) {
                m_lastEndReason = TransferEndReason::Canceled;
            } else if (m_transferInterrupt.load(std::memory_order_relaxed)) {
                m_lastEndReason = m_interruptReason;
                m_lastEndMessage = m_interruptMessage;
            } else {
                m_lastEndReason = TransferEndReason::StallTimeout;
                m_lastEndMessage = error ? *error : QString();
            }
            endTransfer();
            return false;
        }

        RemoteEntry entry;
        if (!m_engine->statEntry(remotePath, &entry, false, error)) {
            endTransfer();
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }

        const QString name = QFileInfo(remotePath).fileName();
        const QString localPath = local.filePath(name);
        if (!downloadPathRecursive(entry, localPath, mode, error, followSymlinks)) {
            endTransfer();
            return false;
        }
    }

    endTransfer();
    m_lastEndReason = TransferEndReason::Completed;
    return true;
}

bool FsRemote::resumeInterruptedTransfer(QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    if (m_backend != FsBackend::Sftp) {
        if (error) {
            *error = trFs("Resume requires SFTP");
        }
        return false;
    }

    auto job = TransferJobStore::loadLatest(m_connectionId);
    if (!job) {
        if (error) {
            *error = trFs("No interrupted transfer to resume");
        }
        return false;
    }

    beginTransfer(job->bytesTotal);
    seedProgressBytes(job->bytesDone);
    bool ok = false;
    if (job->direction == TransferDirection::Upload) {
        ok = transferOneUpload(
            job->localPath, job->remoteFinalPath, TransferWriteMode::ResumeFilepart, error);
    } else {
        ok = transferOneDownload(
            job->remoteFinalPath, job->localPath, TransferWriteMode::ResumeFilepart, error);
    }
    endTransfer();
    return ok;
}

bool FsRemote::discardInterruptedTransfer(QString *error)
{
    auto job = TransferJobStore::loadLatest(m_connectionId);
    if (!job) {
        return true;
    }

    if (job->direction == TransferDirection::Upload && isOpen() && m_backend == FsBackend::Sftp) {
        QString unused;
        m_engine->removeFile(job->filepartPath, &unused);
    } else if (job->direction == TransferDirection::Download) {
        QFile::remove(job->filepartPath);
        QFile::remove(transferMetaPathForFilepart(job->filepartPath));
    }

    m_lastInterruptedJob.reset();
    return TransferJobStore::removeJob(*job, error);
}

bool FsRemote::removePathRecursive(const QString &path, QString *error)
{
    RemoteEntry entry;
    if (!m_engine->statEntry(path, &entry, false, error)) {
        return false;
    }

    if (entry.isSymlink) {
        return m_engine->removeFile(path, error);
    }

    if (entry.isDir) {
        QVector<RemoteEntry> children;
        if (!m_engine->listDirectoryEntries(path, &children, error)) {
            return false;
        }
        for (const RemoteEntry &child : children) {
            if (child.isSymlink) {
                if (!m_engine->removeFile(child.path, error)) {
                    return false;
                }
                continue;
            }
            if (!removePathRecursive(child.path, error)) {
                return false;
            }
        }
        return m_engine->removeDirectory(path, error);
    }

    return m_engine->removeFile(path, error);
}

bool FsRemote::uploadPathRecursive(const QString &localPath,
                                   const QString &remotePath,
                                   TransferWriteMode mode,
                                   QString *error)
{
    if (transferShouldStop(error)) {
        if (m_transferCancel.load(std::memory_order_relaxed)) {
            m_lastEndReason = TransferEndReason::Canceled;
        }
        return false;
    }

    const QFileInfo info(localPath);
    if (info.isSymLink()) {
        QString target;
        if (!Symlink::read(localPath, target, error)) {
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }
        if (!m_engine->createSymlink(target, remotePath, error)) {
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }
        return true;
    }

    if (info.isDir()) {
        QString createError;
        if (!m_engine->createDirectory(remotePath, &createError)) {
            bool existsAsDir = false;
            QString statError;
            if (!(m_engine->isRemoteDirectory(remotePath, &existsAsDir, &statError) &&
                  existsAsDir)) {
                if (error) {
                    *error = createError.isEmpty()
                                 ? trFs("Cannot create remote folder: %1").arg(statError)
                                 : createError;
                }
                m_lastEndReason = TransferEndReason::Failed;
                return false;
            }
        }

        const QDir dir(localPath);
        const auto children =
            dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::System | QDir::NoDotAndDotDot);
        for (const QFileInfo &child : children) {
            const QString childRemote = joinRemotePath(remotePath, child.fileName());
            if (!uploadPathRecursive(child.absoluteFilePath(), childRemote, mode, error)) {
                return false;
            }
        }
        return true;
    }

    return transferOneUpload(localPath, remotePath, mode, error);
}

bool FsRemote::downloadPathRecursive(const RemoteEntry &entry,
                                     const QString &localPath,
                                     TransferWriteMode mode,
                                     QString *error,
                                     bool followSymlinks)
{
    if (transferShouldStop(error)) {
        if (m_transferCancel.load(std::memory_order_relaxed)) {
            m_lastEndReason = TransferEndReason::Canceled;
        }
        return false;
    }

    if (entry.isSymlink) {
        if (followSymlinks) {
            // Open With / follow: sftp_open/scp read through the link to target bytes.
            if (entry.linkIsDir) {
                if (error) {
                    *error = trFs("Cannot follow directory symlink as a file: %1").arg(entry.path);
                }
                m_lastEndReason = TransferEndReason::Failed;
                return false;
            }
            return transferOneDownload(entry.path, localPath, mode, error);
        }

        QString target = entry.linkTarget;
        if (target.isEmpty() && !m_engine->readSymlink(entry.path, target, error)) {
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }
        if (target.isEmpty()) {
            if (error) {
                *error = trFs("Cannot read remote symlink: %1").arg(entry.path);
            }
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }
        if (!Symlink::create({.linkPath = localPath, .target = target}, error)) {
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }
        return true;
    }

    if (entry.isDir) {
        QDir local(localPath);
        if (!local.exists() && !QDir().mkpath(localPath)) {
            if (error) {
                *error = (errno == ENOSPC) ? trFs("Disk full")
                                           : trFs("Cannot create local folder: %1").arg(localPath);
            }
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }

        QVector<RemoteEntry> children;
        if (!m_engine->listDirectoryEntries(entry.path, &children, error)) {
            m_lastEndReason = TransferEndReason::Failed;
            return false;
        }

        for (const RemoteEntry &child : children) {
            if (isTransferFilepartName(child.name)) {
                continue;
            }
            const QString childLocal = QDir(localPath).filePath(child.name);
            if (!downloadPathRecursive(child, childLocal, mode, error, followSymlinks)) {
                return false;
            }
        }
        return true;
    }

    return transferOneDownload(entry.path, localPath, mode, error);
}
