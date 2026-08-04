/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/widgets/IExplorerDetailFactory.h"

class ProcessDetailFactory final : public IExplorerDetailFactory
{
public:
    QDialog *createDetailDialog(QAbstractItemModel *source,
                                const QModelIndex &sourceIndex,
                                QWidget *parent) override;
};
