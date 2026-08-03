// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionFilterProxy.h"

#include "ConnectionModel.h"

#include <QtGlobal>

ConnectionFilterProxy::ConnectionFilterProxy(QObject *parent) : QSortFilterProxyModel(parent) {}

void ConnectionFilterProxy::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_filterText = text;
    endFilterChange(Direction::Rows);
#else
    // begin/endFilterChange require Qt 6.9 / 6.10; CI and many distros are still on 6.6–6.8.
    m_filterText = text;
    invalidateFilter();
#endif
}

void ConnectionFilterProxy::setSourceFilter(std::optional<ConnectionSource> source)
{
    if (m_sourceFilter == source) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_sourceFilter = source;
    endFilterChange(Direction::Rows);
#else
    m_sourceFilter = source;
    invalidateFilter();
#endif
}

bool ConnectionFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *model = sourceModel();
    if (!model) {
        return false;
    }

    const QModelIndex index = model->index(sourceRow, 0, sourceParent);

    if (m_sourceFilter.has_value()) {
        const auto source =
            static_cast<ConnectionSource>(model->data(index, ConnectionModel::SourceRole).toInt());
        if (source != *m_sourceFilter) {
            return false;
        }
    }

    const QString needle = m_filterText.trimmed();
    if (needle.isEmpty()) {
        return true;
    }

    const QString name = model->data(index, ConnectionModel::NameRole).toString();
    const QString host = model->data(index, ConnectionModel::HostRole).toString();
    const QString username = model->data(index, ConnectionModel::UsernameRole).toString();
    const QString alias = model->data(index, ConnectionModel::ConfigAliasRole).toString();
    const QString port = QString::number(model->data(index, ConnectionModel::PortRole).toInt());

    return name.contains(needle, Qt::CaseInsensitive) ||
           host.contains(needle, Qt::CaseInsensitive) ||
           username.contains(needle, Qt::CaseInsensitive) ||
           alias.contains(needle, Qt::CaseInsensitive) ||
           port.contains(needle, Qt::CaseInsensitive);
}
