/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

class QAbstractItemModel;
class QDialog;
class QModelIndex;
class QWidget;

/// Creates a per-domain detail dialog for an explorer row (source-model index).
class IExplorerDetailFactory
{
public:
    virtual ~IExplorerDetailFactory() = default;

    /// @p sourceIndex is already mapped from the filter proxy to the source model.
    /// Return nullptr to skip opening a dialog.
    virtual QDialog *createDetailDialog(QAbstractItemModel *source,
                                        const QModelIndex &sourceIndex,
                                        QWidget *parent) = 0;
};
