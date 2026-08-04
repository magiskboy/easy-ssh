// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ExplorerTableModel.h"

ExplorerTableModel::ExplorerTableModel(QObject *parent) : QAbstractTableModel(parent) {}

Qt::ItemFlags ExplorerTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> ExplorerTableModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {IdRole, QByteArrayLiteral("id")},
    };
}

int ExplorerTableModel::rowOfId(const QString &id) const
{
    return m_idToRow.value(id, -1);
}

void ExplorerTableModel::rebuildIdIndex()
{
    m_idToRow.clear();
    const int rows = rowCount();
    m_idToRow.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        m_idToRow.insert(rowId(row), row);
    }
}

void ExplorerTableModel::clearIdIndex()
{
    m_idToRow.clear();
}

void ExplorerTableModel::beginInsertIdRows(int firstRow, int lastRow)
{
    beginInsertRows(QModelIndex(), firstRow, lastRow);
}

void ExplorerTableModel::endInsertIdRows()
{
    endInsertRows();
    rebuildIdIndex();
}

void ExplorerTableModel::beginRemoveIdRows(int firstRow, int lastRow)
{
    beginRemoveRows(QModelIndex(), firstRow, lastRow);
}

void ExplorerTableModel::endRemoveIdRows()
{
    endRemoveRows();
    rebuildIdIndex();
}

void ExplorerTableModel::emitRowDataChanged(int row, int columnCount)
{
    if (row < 0 || columnCount <= 0) {
        return;
    }
    emit dataChanged(index(row, 0), index(row, columnCount - 1));
}
