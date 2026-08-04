/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/container/ContainerInfo.h"
#include "gui/explorer/ExplorerTableModel.h"

#include <QVector>
#include <optional>

class ContainerTableModel final : public ExplorerTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        RuntimeColumn = 0,
        NameColumn,
        ImageColumn,
        StateColumn,
        CpuColumn,
        MemColumn,
        PidColumn,
        IdColumn,
        ColumnCount,
    };

    enum Roles
    {
        SortValueRole = Qt::UserRole + 10,
    };

    explicit ContainerTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant
    headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QString rowId(int row) const override;

    void applySnapshot(const QVector<ContainerInfo> &containers);
    void clear();

    std::optional<ContainerInfo> containerAt(int row) const;
    std::optional<ContainerInfo> containerById(const QString &id) const;

private:
    static QVariant displayValue(const ContainerInfo &info, int column);

    QVector<ContainerInfo> m_containers;
};
