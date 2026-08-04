/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QString>
#include <QStringList>

/// View-only table model base with stable row ids and structural-update helpers.
class ExplorerTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
    };

    explicit ExplorerTableModel(QObject *parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    virtual QString rowId(int row) const = 0;
    int rowOfId(const QString &id) const;

protected:
    void rebuildIdIndex();
    void clearIdIndex();

    /// Insert contiguous new rows at @p firstRow (caller already prepared data).
    void beginInsertIdRows(int firstRow, int lastRow);
    void endInsertIdRows();

    void beginRemoveIdRows(int firstRow, int lastRow);
    void endRemoveIdRows();

    void emitRowDataChanged(int row, int columnCount);

    QHash<QString, int> m_idToRow;
};
