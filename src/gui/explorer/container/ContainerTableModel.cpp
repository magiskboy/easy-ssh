// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ContainerTableModel.h"

#include "core/explorer/container/ContainerParser.h"

ContainerTableModel::ContainerTableModel(QObject *parent) : ExplorerTableModel(parent) {}

int ContainerTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_containers.size();
}

int ContainerTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant ContainerTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_containers.size()) {
        return {};
    }

    const ContainerInfo &info = m_containers.at(index.row());
    if (role == IdRole) {
        return info.id();
    }
    if (role == Qt::DisplayRole) {
        return displayValue(info, index.column());
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == IdColumn) {
            return info.containerId;
        }
        if (index.column() == MemColumn && !info.memUsage.isEmpty()) {
            return info.memUsage;
        }
        return displayValue(info, index.column());
    }
    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case CpuColumn:
        case MemColumn:
        case PidColumn:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (role == SortValueRole) {
        switch (index.column()) {
        case CpuColumn:
            return info.cpuPercent;
        case MemColumn:
            return info.memPercent;
        case PidColumn:
            return info.pid;
        case StateColumn:
            return ContainerParser::formatStateDisplay(info.state);
        case IdColumn:
            return info.containerId;
        default:
            return displayValue(info, index.column());
        }
    }
    return {};
}

QVariant ContainerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case RuntimeColumn:
        return tr("Runtime");
    case NameColumn:
        return tr("Name");
    case ImageColumn:
        return tr("Image");
    case StateColumn:
        return tr("State");
    case CpuColumn:
        return tr("CPU %");
    case MemColumn:
        return tr("MEM %");
    case PidColumn:
        return tr("PID");
    case IdColumn:
        return tr("ID");
    default:
        return {};
    }
}

QString ContainerTableModel::rowId(int row) const
{
    if (row < 0 || row >= m_containers.size()) {
        return {};
    }
    return m_containers.at(row).id();
}

QVariant ContainerTableModel::displayValue(const ContainerInfo &info, int column)
{
    switch (column) {
    case RuntimeColumn:
        return info.runtime;
    case NameColumn:
        return ContainerParser::displayName(info);
    case ImageColumn:
        return info.image.isEmpty() ? QStringLiteral("—") : info.image;
    case StateColumn:
        return ContainerParser::formatStateDisplay(info.state);
    case CpuColumn:
        return ContainerParser::formatCpuDisplay(info.cpuPercent);
    case MemColumn:
        return ContainerParser::formatMemPercentDisplay(info.memPercent);
    case PidColumn:
        return info.pid > 0 ? QString::number(info.pid) : QStringLiteral("—");
    case IdColumn:
        return ContainerParser::shortId(info.containerId);
    default:
        return {};
    }
}

namespace
{
bool containerEquals(const ContainerInfo &a, const ContainerInfo &b)
{
    return a.runtime == b.runtime && a.containerId == b.containerId && a.name == b.name &&
           a.image == b.image && a.state == b.state && a.pid == b.pid &&
           a.runtimeNamespace == b.runtimeNamespace && a.cpuPercent == b.cpuPercent &&
           a.memPercent == b.memPercent && a.memUsage == b.memUsage;
}
} // namespace

void ContainerTableModel::applySnapshot(const QVector<ContainerInfo> &containers)
{
    QHash<QString, ContainerInfo> incoming;
    incoming.reserve(containers.size());
    QStringList order;
    order.reserve(containers.size());
    for (const ContainerInfo &info : containers) {
        const QString id = info.id();
        if (id.isEmpty() || incoming.contains(id)) {
            continue;
        }
        incoming.insert(id, info);
        order.append(id);
    }

    for (int row = m_containers.size() - 1; row >= 0; --row) {
        if (!incoming.contains(m_containers.at(row).id())) {
            beginRemoveIdRows(row, row);
            m_containers.removeAt(row);
            endRemoveIdRows();
        }
    }

    QVector<ContainerInfo> toInsert;
    toInsert.reserve(order.size());
    for (const QString &id : order) {
        const ContainerInfo &info = incoming.value(id);
        const int row = rowOfId(id);
        if (row >= 0) {
            if (!containerEquals(m_containers.at(row), info)) {
                m_containers[row] = info;
                emitRowDataChanged(row, ColumnCount);
            }
        } else {
            toInsert.append(info);
        }
    }

    if (!toInsert.isEmpty()) {
        const int first = m_containers.size();
        const int last = first + toInsert.size() - 1;
        beginInsertIdRows(first, last);
        m_containers += toInsert;
        endInsertIdRows();
    } else {
        rebuildIdIndex();
    }
}

void ContainerTableModel::clear()
{
    if (m_containers.isEmpty()) {
        return;
    }
    beginResetModel();
    m_containers.clear();
    clearIdIndex();
    endResetModel();
}

std::optional<ContainerInfo> ContainerTableModel::containerAt(int row) const
{
    if (row < 0 || row >= m_containers.size()) {
        return std::nullopt;
    }
    return m_containers.at(row);
}

std::optional<ContainerInfo> ContainerTableModel::containerById(const QString &id) const
{
    return containerAt(rowOfId(id));
}
