/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"

#include <QSortFilterProxyModel>
#include <optional>

class ConnectionFilterProxy final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ConnectionFilterProxy(QObject *parent = nullptr);

    void setFilterText(const QString &text);
    void setSourceFilter(std::optional<ConnectionSource> source);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
    std::optional<ConnectionSource> m_sourceFilter;
};
