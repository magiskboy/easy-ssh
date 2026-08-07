/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QList>
#include <QUuid>

/// Snapshot of currently docked (non-floating) shells for placement decisions.
struct ShellLayoutSnapshot
{
    QList<QUuid> dockedIds;
    QUuid focusedId;
};

/// Where to pin a newly created shell relative to the existing layout.
struct ShellPlacement
{
    QUuid relativeTo;
    /// ads::DockWidgetArea value (kept as int to avoid ADS include in callers).
    int dockArea = 0x10; // CenterDockWidgetArea
};

/**
 * Pure strategy: decide dock area for a new pin without touching ADS widgets.
 * Phase 1: AlternateFocus — split relative to focus, alternating Right / Bottom.
 */
class ShellLayoutPlanner
{
public:
    enum class Mode
    {
        Off,
        AlternateFocus,
    };

    ShellPlacement decide(const ShellLayoutSnapshot &snapshot, Mode mode) const;
};
