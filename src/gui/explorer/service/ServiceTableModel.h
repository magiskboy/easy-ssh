/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/service/ServiceInfo.h"
#include "gui/explorer/ExplorerTableModel.h"

#include <QVector>
#include <optional>

class ServiceTableModel final : public ExplorerTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        UnitColumn = 0,
        ActiveColumn,
        SubColumn,
        EnabledColumn,
        DescriptionColumn,
        PidColumn,
        ColumnCount,
    };

    enum Roles
    {
        SortValueRole = Qt::UserRole + 10,
    };

    explicit ServiceTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant
    headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QString rowId(int row) const override;

    void applySnapshot(const QVector<ServiceInfo> &services);
    void clear();

    std::optional<ServiceInfo> serviceAt(int row) const;
    std::optional<ServiceInfo> serviceById(const QString &id) const;

private:
    static QVariant displayValue(const ServiceInfo &info, int column);

    QVector<ServiceInfo> m_services;
};
