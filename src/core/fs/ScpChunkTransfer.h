/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "FsEngine.h"
#include "TransferTypes.h"

#include <QFile>
#include <QString>

#include <cstddef>
#include <cstdint>

#include <libssh/libssh.h>

class ScpEngine;

/**
 * Tickable single-file SCP transfer for SshIoLoop (Phase 5).
 * Each tick() transfers a bounded number of chunks under a briefly-blocking session.
 * Resume is not supported over SCP.
 */
class ScpChunkTransfer
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

    ScpChunkTransfer() = default;
    ~ScpChunkTransfer();

    ScpChunkTransfer(const ScpChunkTransfer &) = delete;
    ScpChunkTransfer &operator=(const ScpChunkTransfer &) = delete;

    bool startUpload(ScpEngine *engine,
                     ssh_session session,
                     const QString &localPath,
                     const QString &remotePath,
                     const TransferOptions &options,
                     QString *error);
    bool startDownload(ScpEngine *engine,
                       ssh_session session,
                       const QString &remotePath,
                       const QString &localPath,
                       const TransferOptions &options,
                       QString *error);

    TickResult tick(const FsEngine::CancelCheck &shouldCancel,
                    const FsEngine::ProgressNote &onProgress,
                    QString *error);

    void abort();

    bool isActive() const { return m_active; }
    Kind kind() const { return m_kind; }
    qint64 partialBytes() const { return m_bytesDone; }
    QString partialSha256PrefixHex() const { return {}; }
    QString displayName() const { return m_displayName; }

private:
    void closeHandles();
    QString sessionError() const;
    static QString parentRemoteDir(const QString &remotePath);
    static QString remoteBaseName(const QString &remotePath);

    ScpEngine *m_engine = nullptr;
    ssh_session m_session = nullptr;
    ssh_scp m_scp = nullptr;
    Kind m_kind = Kind::Upload;
    bool m_active = false;
    QFile m_localFile;
    QString m_localPath;
    QString m_remotePath;
    QString m_displayName;
    qint64 m_bytesDone = 0;
    qint64 m_bytesTotal = 0;
    uint64_t m_remaining = 0;
};
