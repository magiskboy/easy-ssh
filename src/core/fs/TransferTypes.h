/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/session/SessionTypes.h"

#include <QMetaType>
#include <QString>
#include <QUuid>
#include <QtGlobal>

enum class TransferDirection : quint8
{
    Upload = 0,
    Download = 1,
};

enum class TransferEndReason : quint8
{
    Completed = 0,
    Canceled = 1,
    Interrupted = 2,
    HashMismatch = 3,
    Failed = 4,
    StallTimeout = 5,
};

enum class TransferWriteMode : quint8
{
    /// Write directly to the final path (Open With / SCP / legacy overwrite).
    OverwriteFinal = 0,
    /// Create or truncate `.filepart`, then rename on success.
    FreshFilepart = 1,
    /// Continue an existing `.filepart` after prefix SHA-256 verification.
    ResumeFilepart = 2,
};

struct TransferOptions
{
    TransferWriteMode mode = TransferWriteMode::FreshFilepart;
    /// Expected bytes already present in the partial (ResumeFilepart).
    qint64 resumeOffset = 0;
    /// SHA-256 hex of bytes [0, resumeOffset); required for ResumeFilepart.
    QString sha256PrefixHex;
    /// When set (upload), compared to the streaming hash at completion.
    QString expectedSha256FullHex;
};

struct TransferJob
{
    QUuid connectionId;
    TransferDirection direction = TransferDirection::Upload;
    QString localPath;
    QString remoteFinalPath;
    QString filepartPath;
    qint64 bytesDone = 0;
    qint64 bytesTotal = 0;
    qint64 sourceSize = 0;
    qint64 sourceMtimeUtcMs = 0;
    QString sha256PrefixHex;
    QString sha256FullHex;
    FsBackend backend = FsBackend::Sftp;
    qint64 updatedAtMs = 0;
    TransferEndReason lastReason = TransferEndReason::Interrupted;
    QString lastMessage;
};

Q_DECLARE_METATYPE(TransferJob)
Q_DECLARE_METATYPE(TransferDirection)
Q_DECLARE_METATYPE(TransferEndReason)

inline QString transferFilepartName(const QString &finalName)
{
    return finalName + QStringLiteral(".filepart");
}

inline bool isTransferFilepartName(const QString &name)
{
    return name.endsWith(QLatin1String(".filepart")) ||
           name.endsWith(QLatin1String(".filepart.meta.json"));
}

inline QString transferFilepartPathForFinal(const QString &finalPath)
{
    return finalPath + QStringLiteral(".filepart");
}

inline QString transferMetaPathForFilepart(const QString &filepartPath)
{
    return filepartPath + QStringLiteral(".meta.json");
}
