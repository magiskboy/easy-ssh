/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/process/ProcessInfo.h"
#include "gui/explorer/ExplorerTableModel.h"

#include <QVector>
#include <optional>

class ProcessTableModel final : public ExplorerTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        PidColumn = 0,
        UserColumn,
        CpuColumn,
        MemColumn,
        StateColumn,
        CommandColumn,
        ColumnCount,
    };

    enum Roles
    {
        SortValueRole = Qt::UserRole + 10,
    };

    explicit ProcessTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant
    headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QString rowId(int row) const override;

    void applySnapshot(const QVector<ProcessInfo> &processes);
    void clear();

    std::optional<ProcessInfo> processAt(int row) const;
    std::optional<ProcessInfo> processById(const QString &id) const;

private:
    static QVariant displayValue(const ProcessInfo &info, int column);

    QVector<ProcessInfo> m_processes;
};
