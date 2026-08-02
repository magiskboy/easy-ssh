/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "TransferTypes.h"

#include <optional>

class TransferJobStore
{
public:
    static QString
    jobKey(TransferDirection direction, const QString &localPath, const QString &remoteFinalPath);

    static std::optional<TransferJob> load(const QUuid &connectionId, const QString &key);
    static std::optional<TransferJob> loadForPaths(const QUuid &connectionId,
                                                   TransferDirection direction,
                                                   const QString &localPath,
                                                   const QString &remoteFinalPath);

    /// Most recently updated interrupted job for this connection, if any.
    static std::optional<TransferJob> loadLatest(const QUuid &connectionId);

    static bool save(const TransferJob &job, QString *error = nullptr);
    static bool remove(const QUuid &connectionId, const QString &key, QString *error = nullptr);
    static bool removeJob(const TransferJob &job, QString *error = nullptr);
    static void removeAllForConnection(const QUuid &connectionId);

    static QString connectionDir(const QUuid &connectionId);
};
