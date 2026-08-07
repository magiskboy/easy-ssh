// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ShellLayoutPlanner.h"

namespace
{
// Match ads::DockWidgetArea without depending on ADS headers here.
constexpr int kCenter = 0x10;
constexpr int kRight = 0x02;
constexpr int kBottom = 0x08;
} // namespace

ShellPlacement ShellLayoutPlanner::decide(const ShellLayoutSnapshot &snapshot, Mode mode) const
{
    ShellPlacement placement;
    placement.dockArea = kCenter;

    if (mode == Mode::Off || snapshot.dockedIds.isEmpty()) {
        return placement;
    }

    QUuid relative = snapshot.focusedId;
    if (relative.isNull() || !snapshot.dockedIds.contains(relative)) {
        relative = snapshot.dockedIds.first();
    }

    placement.relativeTo = relative;
    // Odd count → vertical split (Right); even → horizontal (Bottom).
    placement.dockArea = (snapshot.dockedIds.size() % 2 == 1) ? kRight : kBottom;
    return placement;
}
