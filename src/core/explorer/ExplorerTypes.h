/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QStringList>

/// Host / tool availability for view-only explorers.
enum class ExplorerCapability
{
    Checking,
    Available,
    Unavailable,
    PermissionDenied,
    Error,
};

/// How an explorer source pushes rows into the table model.
enum class ExplorerUpdateMode
{
    Snapshot,
    Delta,
    Append,
};

/// Keyed delta for explorers that can avoid full snapshot replace.
struct ExplorerDelta
{
    QStringList removedIds;
};
