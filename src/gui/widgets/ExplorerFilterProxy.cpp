// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ExplorerFilterProxy.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QtGlobal>

ExplorerFilterProxy::ExplorerFilterProxy(QObject *parent) : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void ExplorerFilterProxy::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    m_filterText = text;
    invalidateRowFilter();
}

void ExplorerFilterProxy::setSearchColumns(const QList<int> &columns)
{
    if (m_searchColumns == columns) {
        return;
    }
    m_searchColumns = columns;
    invalidateRowFilter();
}

void ExplorerFilterProxy::setColumnFilters(const QHash<int, QVariant> &filters)
{
    if (m_columnFilters == filters) {
        return;
    }
    m_columnFilters = filters;
    invalidateRowFilter();
}

void ExplorerFilterProxy::setColumnFilter(int column, const QVariant &value)
{
    if (!value.isValid() ||
        (value.metaType().id() == QMetaType::QString && value.toString().isEmpty())) {
        clearColumnFilter(column);
        return;
    }
    if (m_columnFilters.value(column) == value) {
        return;
    }
    m_columnFilters.insert(column, value);
    invalidateRowFilter();
}

void ExplorerFilterProxy::clearColumnFilter(int column)
{
    if (!m_columnFilters.contains(column)) {
        return;
    }
    m_columnFilters.remove(column);
    invalidateRowFilter();
}

void ExplorerFilterProxy::setRowPredicate(RowPredicate predicate)
{
    m_rowPredicate = std::move(predicate);
    invalidateRowFilter();
}

void ExplorerFilterProxy::invalidateRowFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(Direction::Rows);
#else
    invalidateFilter();
#endif
}

bool ExplorerFilterProxy::textMatches(const QAbstractItemModel *model,
                                      int sourceRow,
                                      const QModelIndex &sourceParent) const
{
    const QString needle = m_filterText.trimmed();
    if (needle.isEmpty()) {
        return true;
    }

    const int columnCount = model->columnCount(sourceParent);
    if (m_searchColumns.isEmpty()) {
        for (int column = 0; column < columnCount; ++column) {
            const QModelIndex index = model->index(sourceRow, column, sourceParent);
            if (model->data(index, Qt::DisplayRole)
                    .toString()
                    .contains(needle, Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }

    for (int column : m_searchColumns) {
        if (column < 0 || column >= columnCount) {
            continue;
        }
        const QModelIndex index = model->index(sourceRow, column, sourceParent);
        if (model->data(index, Qt::DisplayRole).toString().contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool ExplorerFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *model = sourceModel();
    if (!model) {
        return false;
    }

    if (!m_columnFilters.isEmpty()) {
        for (auto it = m_columnFilters.constBegin(); it != m_columnFilters.constEnd(); ++it) {
            const QModelIndex index = model->index(sourceRow, it.key(), sourceParent);
            const QVariant cell = model->data(index, Qt::DisplayRole);
            const QVariant &expected = it.value();
            if (expected.metaType().id() == QMetaType::QString ||
                cell.metaType().id() == QMetaType::QString) {
                if (cell.toString().compare(expected.toString(), Qt::CaseInsensitive) != 0) {
                    return false;
                }
            } else if (cell != expected) {
                return false;
            }
        }
    }

    if (!textMatches(model, sourceRow, sourceParent)) {
        return false;
    }

    if (m_rowPredicate) {
        const QModelIndex index = model->index(sourceRow, 0, sourceParent);
        if (!m_rowPredicate(index)) {
            return false;
        }
    }

    return true;
}

bool ExplorerFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const QVariant leftData = sourceModel()->data(left, sortRole());
    const QVariant rightData = sourceModel()->data(right, sortRole());

    if (leftData.userType() == QMetaType::QDateTime &&
        rightData.userType() == QMetaType::QDateTime) {
        return leftData.toDateTime() < rightData.toDateTime();
    }

    if (leftData.typeId() == QMetaType::Double || rightData.typeId() == QMetaType::Double ||
        leftData.typeId() == QMetaType::Float || rightData.typeId() == QMetaType::Float) {
        return leftData.toDouble() < rightData.toDouble();
    }

    if (leftData.canConvert<qlonglong>() && rightData.canConvert<qlonglong>() &&
        (leftData.typeId() == QMetaType::Int || leftData.typeId() == QMetaType::LongLong ||
         leftData.typeId() == QMetaType::UInt || leftData.typeId() == QMetaType::ULongLong ||
         rightData.typeId() == QMetaType::Int || rightData.typeId() == QMetaType::LongLong ||
         rightData.typeId() == QMetaType::UInt || rightData.typeId() == QMetaType::ULongLong)) {
        return leftData.toLongLong() < rightData.toLongLong();
    }

    return QString::localeAwareCompare(leftData.toString(), rightData.toString()) < 0;
}
