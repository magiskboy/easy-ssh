/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QHash>
#include <QList>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVariant>

#include <functional>

/// Search + structured row filtering for ExplorerListWidget.
class ExplorerFilterProxy final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using RowPredicate = std::function<bool(const QModelIndex &sourceIndex)>;

    explicit ExplorerFilterProxy(QObject *parent = nullptr);

    void setFilterText(const QString &text);
    QString filterText() const { return m_filterText; }

    /// Columns searched with substring match (DisplayRole). Empty = all columns.
    void setSearchColumns(const QList<int> &columns);
    QList<int> searchColumns() const { return m_searchColumns; }

    /// Exact match of DisplayRole (or equality of QVariant) per column. Cleared keys removed.
    void setColumnFilters(const QHash<int, QVariant> &filters);
    void setColumnFilter(int column, const QVariant &value);
    void clearColumnFilter(int column);
    QHash<int, QVariant> columnFilters() const { return m_columnFilters; }

    /// Additional predicate on the source row's column-0 index. Empty predicate = accept.
    void setRowPredicate(RowPredicate predicate);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    void invalidateRowFilter();
    bool textMatches(const QAbstractItemModel *model,
                     int sourceRow,
                     const QModelIndex &sourceParent) const;

    QString m_filterText;
    QList<int> m_searchColumns;
    QHash<int, QVariant> m_columnFilters;
    RowPredicate m_rowPredicate;
};
