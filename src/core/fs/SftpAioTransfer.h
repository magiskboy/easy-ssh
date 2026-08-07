/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "FsEngine.h"
#include "TransferTypes.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QString>

#include <cstddef>
#include <cstdint>

#include <libssh/sftp.h>

class SftpEngine;

/**
 * Tickable single-file SFTP transfer for SshIoLoop (Phase 3).
 *
 * Each tick() transfers a bounded number of chunks. Caller must run tick() under
 * a briefly-blocking ssh_session (FsRemote::withBlockingSession). Yields between
 * ticks so dopoll / shell stay responsive. Avoids nonblocking sftp_aio + SSH_AGAIN,
 * which can hang when the SFTP channel is only pumped by ssh_event_dopoll.
 */
class SftpAioTransfer
{
public:
    enum class Kind : uint8_t
    {
        Upload,
        Download,
    };

    enum class TickResult : uint8_t
    {
        Again,
        Done,
        Failed,
    };

    SftpAioTransfer() = default;
    ~SftpAioTransfer();

    SftpAioTransfer(const SftpAioTransfer &) = delete;
    SftpAioTransfer &operator=(const SftpAioTransfer &) = delete;

    bool startUpload(SftpEngine *engine,
                     const QString &localPath,
                     const QString &remotePath,
                     const TransferOptions &options,
                     QString *error);
    bool startDownload(SftpEngine *engine,
                       const QString &remotePath,
                       const QString &localPath,
                       const TransferOptions &options,
                       QString *error);

    /// Caller must hold a blocking ssh_session for the duration of this call.
    TickResult tick(const FsEngine::CancelCheck &shouldCancel,
                    const FsEngine::ProgressNote &onProgress,
                    QString *error);

    void abort();

    bool isActive() const { return m_active; }
    Kind kind() const { return m_kind; }
    qint64 partialBytes() const { return m_bytesDone; }
    QString partialSha256PrefixHex() const;
    QString displayName() const { return m_displayName; }

private:
    void closeHandles();
    bool finishUpload(QString *error);
    bool finishDownload(QString *error);
    TickResult tickUploadChunk(const FsEngine::ProgressNote &onProgress, QString *error);
    TickResult tickDownloadChunk(const FsEngine::ProgressNote &onProgress, QString *error);
    void persistPartialHints();
    size_t chunkSize() const;

    SftpEngine *m_engine = nullptr;
    Kind m_kind = Kind::Upload;
    bool m_active = false;
    TransferOptions m_options;
    QString m_localPath;
    QString m_remoteFinalPath;
    QString m_writePath;
    QString m_displayName;
    bool m_useFilepart = false;

    sftp_file m_remoteFile = nullptr;
    QFile m_localFile;
    QCryptographicHash m_running{QCryptographicHash::Sha256};

    qint64 m_bytesDone = 0;
    qint64 m_totalSize = 0;

    size_t m_chunk = 16384;
    QByteArray m_buf;

    qint64 m_partialBytes = 0;
    QString m_partialHash;
};
