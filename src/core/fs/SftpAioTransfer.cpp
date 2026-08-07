// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SftpAioTransfer.h"

#include "SftpEngine.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <fcntl.h>
#include <sys/stat.h>

#ifndef S_IRUSR
#define S_IRUSR 00400u
#define S_IWUSR 00200u
#define S_IRGRP 00040u
#define S_IROTH 00004u
#endif

namespace
{
constexpr size_t kFallbackChunk = 16384;
/// Chunks per idle tick — keep short so shell stays responsive.
constexpr int kChunksPerTick = 4;

QString trAio(const char *text)
{
    return QCoreApplication::translate("SftpAioTransfer", text);
}
} // namespace

SftpAioTransfer::~SftpAioTransfer()
{
    abort();
}

void SftpAioTransfer::closeHandles()
{
    if (m_remoteFile != nullptr) {
        sftp_close(m_remoteFile);
        m_remoteFile = nullptr;
    }
    if (m_localFile.isOpen()) {
        m_localFile.close();
    }
}

void SftpAioTransfer::abort()
{
    if (!m_active && m_remoteFile == nullptr) {
        return;
    }
    persistPartialHints();
    closeHandles();
    m_active = false;
    m_engine = nullptr;
}

QString SftpAioTransfer::partialSha256PrefixHex() const
{
    return m_partialHash;
}

void SftpAioTransfer::persistPartialHints()
{
    m_partialBytes = m_bytesDone;
    if (m_bytesDone <= 0 || m_engine == nullptr) {
        m_partialHash.clear();
        return;
    }
    if (m_kind == Kind::Upload) {
        QString hex;
        if (m_engine->hashLocalPrefix(m_localPath, m_bytesDone, hex, nullptr)) {
            m_partialHash = hex;
        }
    } else {
        m_localFile.flush();
        QString hex;
        if (m_engine->hashLocalPrefix(m_writePath, m_bytesDone, hex, nullptr)) {
            m_partialHash = hex;
        }
    }
}

size_t SftpAioTransfer::chunkSize() const
{
    return m_chunk > 0 ? m_chunk : kFallbackChunk;
}

bool SftpAioTransfer::startUpload(SftpEngine *engine,
                                  const QString &localPath,
                                  const QString &remotePath,
                                  const TransferOptions &options,
                                  QString *error)
{
    abort();
    if (engine == nullptr || !engine->isOpen()) {
        if (error) {
            *error = trAio("SFTP is not available");
        }
        return false;
    }

    m_engine = engine;
    m_kind = Kind::Upload;
    m_options = options;
    m_localPath = localPath;
    m_remoteFinalPath = remotePath;
    m_useFilepart = options.mode != TransferWriteMode::OverwriteFinal;
    m_writePath = m_useFilepart ? transferFilepartPathForFinal(remotePath) : remotePath;
    m_displayName = QFileInfo(localPath).fileName();
    m_running.reset();
    m_chunk = kFallbackChunk;

    m_localFile.setFileName(localPath);
    if (!m_localFile.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = trAio("Cannot open local file: %1")
                         .arg(SftpEngine::localIoErrorMessage(m_localFile.errorString()));
        }
        m_engine = nullptr;
        return false;
    }

    m_totalSize = m_localFile.size();
    qint64 offset = 0;

    if (options.mode == TransferWriteMode::ResumeFilepart) {
        offset = options.resumeOffset;
        if (offset < 0 || offset > m_totalSize) {
            if (error) {
                *error = trAio("Invalid resume offset");
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        qint64 remoteSize = 0;
        if (!engine->remoteFileSize(m_writePath, &remoteSize, error) || remoteSize != offset) {
            if (error && error->isEmpty()) {
                *error = trAio("Remote partial size does not match resume offset");
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        QString localPrefix;
        if (!engine->hashLocalPrefix(localPath, offset, localPrefix, error)) {
            closeHandles();
            m_engine = nullptr;
            return false;
        }
        if (localPrefix.compare(options.sha256PrefixHex, Qt::CaseInsensitive) != 0) {
            if (error) {
                *error = trAio("Local file changed since the interrupted transfer (hash mismatch)");
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        QString remotePrefix;
        if (!engine->hashRemotePrefix(m_writePath, offset, remotePrefix, error)) {
            closeHandles();
            m_engine = nullptr;
            return false;
        }
        if (remotePrefix.compare(options.sha256PrefixHex, Qt::CaseInsensitive) != 0) {
            if (error) {
                *error = trAio("Remote partial file is corrupt (hash mismatch)");
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        if (!engine->feedHashFromLocal(m_localFile, offset, &m_running, error)) {
            closeHandles();
            m_engine = nullptr;
            return false;
        }
        if (!m_localFile.seek(offset)) {
            if (error) {
                *error = trAio("Cannot seek local file: %1")
                             .arg(SftpEngine::localIoErrorMessage(m_localFile.errorString()));
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }
    } else if (!m_localFile.seek(0)) {
        if (error) {
            *error = trAio("Cannot seek local file: %1")
                         .arg(SftpEngine::localIoErrorMessage(m_localFile.errorString()));
        }
        closeHandles();
        m_engine = nullptr;
        return false;
    }

    int access = O_WRONLY | O_CREAT;
    if (options.mode != TransferWriteMode::ResumeFilepart) {
        access |= O_TRUNC;
    }
    const QByteArray remote = m_writePath.toUtf8();
    m_remoteFile = sftp_open(
        engine->m_sftp, remote.constData(), access, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (m_remoteFile == nullptr) {
        if (error) {
            *error =
                trAio("Cannot open remote file for writing: %1").arg(engine->sftpErrorMessage());
        }
        closeHandles();
        m_engine = nullptr;
        return false;
    }

    if (options.mode == TransferWriteMode::ResumeFilepart) {
        if (sftp_seek64(m_remoteFile, static_cast<uint64_t>(offset)) < 0) {
            if (error) {
                *error = trAio("Cannot seek remote file: %1").arg(engine->sftpErrorMessage());
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }
    }

    // Keep file blocking — each tick does a short sync chunk then yields to IoLoop.
    sftp_file_set_blocking(m_remoteFile);

    if (sftp_limits_t lim = sftp_limits(engine->m_sftp)) {
        if (lim->max_write_length > 0) {
            m_chunk = static_cast<size_t>(
                qMin(static_cast<quint64>(kFallbackChunk), lim->max_write_length));
        }
        sftp_limits_free(lim);
    }

    m_buf.resize(static_cast<int>(chunkSize()));
    m_bytesDone = offset;
    m_active = true;
    return true;
}

bool SftpAioTransfer::startDownload(SftpEngine *engine,
                                    const QString &remotePath,
                                    const QString &localPath,
                                    const TransferOptions &options,
                                    QString *error)
{
    abort();
    if (engine == nullptr || !engine->isOpen()) {
        if (error) {
            *error = trAio("SFTP is not available");
        }
        return false;
    }

    m_engine = engine;
    m_kind = Kind::Download;
    m_options = options;
    m_localPath = localPath;
    m_remoteFinalPath = remotePath;
    m_useFilepart = options.mode != TransferWriteMode::OverwriteFinal;
    m_writePath = m_useFilepart ? transferFilepartPathForFinal(localPath) : localPath;
    m_displayName = QFileInfo(remotePath).fileName();
    m_running.reset();
    m_chunk = kFallbackChunk;

    if (!engine->remoteFileSize(remotePath, &m_totalSize, error)) {
        m_engine = nullptr;
        return false;
    }

    const QByteArray remote = remotePath.toUtf8();
    m_remoteFile = sftp_open(engine->m_sftp, remote.constData(), O_RDONLY, 0);
    if (m_remoteFile == nullptr) {
        if (error) {
            *error =
                trAio("Cannot open remote file for reading: %1").arg(engine->sftpErrorMessage());
        }
        m_engine = nullptr;
        return false;
    }

    qint64 offset = 0;
    QIODevice::OpenMode localMode = QIODevice::WriteOnly | QIODevice::Truncate;

    if (options.mode == TransferWriteMode::ResumeFilepart) {
        offset = options.resumeOffset;
        if (offset < 0 || offset > m_totalSize) {
            if (error) {
                *error = trAio("Invalid resume offset");
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        const QFileInfo partInfo(m_writePath);
        if (!partInfo.exists() || partInfo.size() != offset) {
            if (error) {
                *error = trAio("Local partial size does not match resume offset");
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        QString localPrefix;
        if (!engine->hashLocalPrefix(m_writePath, offset, localPrefix, error)) {
            closeHandles();
            m_engine = nullptr;
            return false;
        }
        if (localPrefix.compare(options.sha256PrefixHex, Qt::CaseInsensitive) != 0) {
            if (error) {
                *error = trAio("Local partial file is corrupt (hash mismatch)");
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        QFile seed(m_writePath);
        if (!seed.open(QIODevice::ReadOnly) ||
            !engine->feedHashFromLocal(seed, offset, &m_running, error)) {
            closeHandles();
            m_engine = nullptr;
            return false;
        }

        if (sftp_seek64(m_remoteFile, static_cast<uint64_t>(offset)) < 0) {
            if (error) {
                *error = trAio("Cannot seek remote file: %1").arg(engine->sftpErrorMessage());
            }
            closeHandles();
            m_engine = nullptr;
            return false;
        }
        localMode = QIODevice::WriteOnly | QIODevice::Append;
    }

    m_localFile.setFileName(m_writePath);
    if (!m_localFile.open(localMode)) {
        if (error) {
            *error = trAio("Cannot open local file for writing: %1")
                         .arg(SftpEngine::localIoErrorMessage(m_localFile.errorString()));
        }
        closeHandles();
        m_engine = nullptr;
        return false;
    }

    sftp_file_set_blocking(m_remoteFile);

    if (sftp_limits_t lim = sftp_limits(engine->m_sftp)) {
        if (lim->max_read_length > 0) {
            m_chunk = static_cast<size_t>(
                qMin(static_cast<quint64>(kFallbackChunk), lim->max_read_length));
        }
        sftp_limits_free(lim);
    }

    m_buf.resize(static_cast<int>(chunkSize()));
    m_bytesDone = offset;
    m_active = true;
    return true;
}

SftpAioTransfer::TickResult
SftpAioTransfer::tickUploadChunk(const FsEngine::ProgressNote &onProgress, QString *error)
{
    if (m_bytesDone >= m_totalSize) {
        if (!finishUpload(error)) {
            closeHandles();
            m_active = false;
            return TickResult::Failed;
        }
        closeHandles();
        m_active = false;
        return TickResult::Done;
    }

    const qint64 want = qMin(static_cast<qint64>(chunkSize()), m_totalSize - m_bytesDone);
    if (m_buf.size() < want) {
        m_buf.resize(static_cast<int>(want));
    }

    const qint64 nread = m_localFile.read(m_buf.data(), want);
    if (nread < 0) {
        if (error) {
            *error = trAio("Cannot read local file: %1")
                         .arg(SftpEngine::localIoErrorMessage(m_localFile.errorString()));
        }
        persistPartialHints();
        closeHandles();
        m_active = false;
        return TickResult::Failed;
    }
    if (nread == 0) {
        // Unexpected EOF before totalSize — finish with what we have / fail size check.
        if (!finishUpload(error)) {
            closeHandles();
            m_active = false;
            return TickResult::Failed;
        }
        closeHandles();
        m_active = false;
        return TickResult::Done;
    }

    qint64 remaining = nread;
    const char *ptr = m_buf.constData();
    while (remaining > 0) {
        const ssize_t nwritten = sftp_write(m_remoteFile, ptr, static_cast<size_t>(remaining));
        if (nwritten < 0) {
            if (error) {
                const QString detail = m_engine->sftpErrorMessage();
                *error = detail.isEmpty() ? trAio("Cannot write remote file")
                                          : trAio("Cannot write remote file: %1").arg(detail);
            }
            persistPartialHints();
            closeHandles();
            m_active = false;
            return TickResult::Failed;
        }
        m_running.addData(QByteArrayView(ptr, static_cast<qsizetype>(nwritten)));
        ptr += nwritten;
        remaining -= nwritten;
        m_bytesDone += nwritten;
        if (onProgress) {
            onProgress(nwritten, m_displayName);
        }
    }
    return TickResult::Again;
}

SftpAioTransfer::TickResult
SftpAioTransfer::tickDownloadChunk(const FsEngine::ProgressNote &onProgress, QString *error)
{
    if (m_bytesDone >= m_totalSize) {
        if (!finishDownload(error)) {
            closeHandles();
            m_active = false;
            return TickResult::Failed;
        }
        closeHandles();
        m_active = false;
        return TickResult::Done;
    }

    const size_t want =
        static_cast<size_t>(qMin(static_cast<qint64>(chunkSize()), m_totalSize - m_bytesDone));
    if (m_buf.size() < static_cast<int>(want)) {
        m_buf.resize(static_cast<int>(want));
    }

    const ssize_t nbytes = sftp_read(m_remoteFile, m_buf.data(), want);
    if (nbytes < 0) {
        if (error) {
            const QString detail = m_engine->sftpErrorMessage();
            *error = detail.isEmpty() ? trAio("Cannot read remote file")
                                      : trAio("Cannot read remote file: %1").arg(detail);
        }
        persistPartialHints();
        closeHandles();
        m_active = false;
        return TickResult::Failed;
    }
    if (nbytes == 0) {
        // EOF — verify size in finishDownload.
        if (!finishDownload(error)) {
            closeHandles();
            m_active = false;
            return TickResult::Failed;
        }
        closeHandles();
        m_active = false;
        return TickResult::Done;
    }

    if (m_localFile.write(m_buf.constData(), nbytes) != nbytes) {
        if (error) {
            *error = trAio("Cannot write local file: %1")
                         .arg(SftpEngine::localIoErrorMessage(m_localFile.errorString()));
        }
        persistPartialHints();
        closeHandles();
        m_active = false;
        return TickResult::Failed;
    }

    m_running.addData(QByteArrayView(m_buf.constData(), static_cast<qsizetype>(nbytes)));
    m_bytesDone += nbytes;
    if (onProgress) {
        onProgress(nbytes, m_displayName);
    }
    return TickResult::Again;
}

bool SftpAioTransfer::finishUpload(QString *error)
{
    if (m_remoteFile != nullptr) {
        if (sftp_close(m_remoteFile) != SSH_OK) {
            if (error) {
                const QString detail = m_engine->sftpErrorMessage();
                *error = detail.isEmpty() ? trAio("Cannot close remote file")
                                          : trAio("Cannot close remote file: %1").arg(detail);
            }
            m_remoteFile = nullptr;
            persistPartialHints();
            return false;
        }
        m_remoteFile = nullptr;
    }
    m_localFile.close();

    if (m_bytesDone != m_totalSize) {
        if (error) {
            *error = trAio("Upload size mismatch");
        }
        persistPartialHints();
        return false;
    }

    const QString streamedHex = QString::fromLatin1(m_running.result().toHex());
    QString expectedFull = m_options.expectedSha256FullHex;
    if (expectedFull.isEmpty()) {
        if (!m_engine->hashLocalFile(m_localPath, expectedFull, error)) {
            persistPartialHints();
            return false;
        }
    }
    if (streamedHex.compare(expectedFull, Qt::CaseInsensitive) != 0) {
        if (error) {
            *error = trAio("Upload hash mismatch");
        }
        persistPartialHints();
        return false;
    }

    if (m_useFilepart) {
        QString unused;
        m_engine->removeFile(m_remoteFinalPath, &unused);
        if (!m_engine->renamePath(m_writePath, m_remoteFinalPath, error)) {
            persistPartialHints();
            return false;
        }
    }

    m_partialBytes = m_bytesDone;
    m_partialHash = streamedHex;
    return true;
}

bool SftpAioTransfer::finishDownload(QString *error)
{
    if (m_remoteFile != nullptr) {
        if (sftp_close(m_remoteFile) != SSH_OK) {
            if (error) {
                const QString detail = m_engine->sftpErrorMessage();
                *error = detail.isEmpty() ? trAio("Cannot close remote file")
                                          : trAio("Cannot close remote file: %1").arg(detail);
            }
            m_remoteFile = nullptr;
            persistPartialHints();
            return false;
        }
        m_remoteFile = nullptr;
    }
    m_localFile.close();

    if (m_bytesDone != m_totalSize) {
        if (error) {
            *error = trAio("Download size mismatch");
        }
        persistPartialHints();
        return false;
    }

    const QString streamedHex = QString::fromLatin1(m_running.result().toHex());
    QString verifyHex;
    if (!m_engine->hashLocalPrefix(m_writePath, m_bytesDone, verifyHex, error) ||
        verifyHex.compare(streamedHex, Qt::CaseInsensitive) != 0) {
        if (error && error->isEmpty()) {
            *error = trAio("Download hash mismatch");
        }
        persistPartialHints();
        return false;
    }

    if (m_useFilepart) {
        if (QFile::exists(m_localPath) && !QFile::remove(m_localPath)) {
            if (error) {
                *error = trAio("Cannot replace local file: %1").arg(m_localPath);
            }
            persistPartialHints();
            return false;
        }
        if (!QFile::rename(m_writePath, m_localPath)) {
            if (error) {
                *error = trAio("Cannot finalize downloaded file: %1").arg(m_localPath);
            }
            persistPartialHints();
            return false;
        }
    }

    m_partialBytes = m_bytesDone;
    m_partialHash = streamedHex;
    return true;
}

SftpAioTransfer::TickResult SftpAioTransfer::tick(const FsEngine::CancelCheck &shouldCancel,
                                                  const FsEngine::ProgressNote &onProgress,
                                                  QString *error)
{
    if (!m_active || m_engine == nullptr) {
        return TickResult::Failed;
    }

    if (shouldCancel && shouldCancel(error)) {
        persistPartialHints();
        closeHandles();
        m_active = false;
        return TickResult::Failed;
    }

    TickResult last = TickResult::Again;
    for (int i = 0; i < kChunksPerTick; ++i) {
        if (shouldCancel && shouldCancel(error)) {
            persistPartialHints();
            closeHandles();
            m_active = false;
            return TickResult::Failed;
        }
        last = (m_kind == Kind::Upload) ? tickUploadChunk(onProgress, error)
                                        : tickDownloadChunk(onProgress, error);
        if (last != TickResult::Again) {
            break;
        }
    }
    return last;
}
