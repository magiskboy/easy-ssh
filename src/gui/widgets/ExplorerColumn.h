/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QHeaderView>
#include <Qt>

/// View-side column configuration for ExplorerListWidget (resize / initial sort).
/// Header titles come from the source model's headerData().
struct ExplorerColumn
{
    bool stretch = false;
    int defaultWidth = -1;
    QHeaderView::ResizeMode resizeMode = QHeaderView::Interactive;
    bool defaultSort = false;
    Qt::SortOrder defaultSortOrder = Qt::AscendingOrder;
};
