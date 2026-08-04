// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ProcessTableModel.h"

#include "core/explorer/process/ProcessParser.h"

ProcessTableModel::ProcessTableModel(QObject *parent) : ExplorerTableModel(parent) {}

int ProcessTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_processes.size();
}

int ProcessTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant ProcessTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_processes.size()) {
        return {};
    }

    const ProcessInfo &info = m_processes.at(index.row());
    if (role == IdRole) {
        return info.id();
    }
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        return displayValue(info, index.column());
    }
    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case PidColumn:
        case CpuColumn:
        case MemColumn:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (role == SortValueRole) {
        switch (index.column()) {
        case PidColumn:
            return info.pid;
        case CpuColumn:
            return info.cpuPercent;
        case MemColumn:
            return info.memPercent;
        case StateColumn:
            return ProcessParser::formatStateDisplay(info.stateCode);
        default:
            return displayValue(info, index.column());
        }
    }
    return {};
}

QVariant ProcessTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case PidColumn:
        return tr("PID");
    case UserColumn:
        return tr("User");
    case CpuColumn:
        return tr("CPU %");
    case MemColumn:
        return tr("MEM %");
    case StateColumn:
        return tr("State");
    case CommandColumn:
        return tr("Command");
    default:
        return {};
    }
}

QString ProcessTableModel::rowId(int row) const
{
    if (row < 0 || row >= m_processes.size()) {
        return {};
    }
    return m_processes.at(row).id();
}

QVariant ProcessTableModel::displayValue(const ProcessInfo &info, int column)
{
    switch (column) {
    case PidColumn:
        return QString::number(info.pid);
    case UserColumn:
        return info.user;
    case CpuColumn:
        return QString::number(info.cpuPercent, 'f', 1);
    case MemColumn:
        return QString::number(info.memPercent, 'f', 1);
    case StateColumn:
        return ProcessParser::formatStateDisplay(info.stateCode);
    case CommandColumn:
        return info.command.isEmpty() ? info.comm : info.command;
    default:
        return {};
    }
}

namespace
{
bool processEquals(const ProcessInfo &a, const ProcessInfo &b)
{
    return a.pid == b.pid && a.ppid == b.ppid && a.uid == b.uid && a.user == b.user &&
           a.cpuPercent == b.cpuPercent && a.memPercent == b.memPercent &&
           a.stateCode == b.stateCode && a.nice == b.nice && a.priority == b.priority &&
           a.elapsedSeconds == b.elapsedSeconds && a.cpuTime == b.cpuTime && a.rssKiB == b.rssKiB &&
           a.vszKiB == b.vszKiB && a.comm == b.comm && a.command == b.command;
}
} // namespace

void ProcessTableModel::applySnapshot(const QVector<ProcessInfo> &processes)
{
    QHash<QString, ProcessInfo> incoming;
    incoming.reserve(processes.size());
    QStringList order;
    order.reserve(processes.size());
    for (const ProcessInfo &info : processes) {
        const QString id = info.id();
        if (id.isEmpty() || incoming.contains(id)) {
            continue;
        }
        incoming.insert(id, info);
        order.append(id);
    }

    for (int row = m_processes.size() - 1; row >= 0; --row) {
        if (!incoming.contains(m_processes.at(row).id())) {
            beginRemoveIdRows(row, row);
            m_processes.removeAt(row);
            endRemoveIdRows();
        }
    }

    QVector<ProcessInfo> toInsert;
    toInsert.reserve(order.size());
    for (const QString &id : order) {
        const ProcessInfo &info = incoming.value(id);
        const int row = rowOfId(id);
        if (row >= 0) {
            if (!processEquals(m_processes.at(row), info)) {
                m_processes[row] = info;
                emitRowDataChanged(row, ColumnCount);
            }
        } else {
            toInsert.append(info);
        }
    }

    if (!toInsert.isEmpty()) {
        const int first = m_processes.size();
        const int last = first + toInsert.size() - 1;
        beginInsertIdRows(first, last);
        m_processes += toInsert;
        endInsertIdRows();
    } else {
        rebuildIdIndex();
    }
}

void ProcessTableModel::clear()
{
    if (m_processes.isEmpty()) {
        return;
    }
    beginResetModel();
    m_processes.clear();
    clearIdIndex();
    endResetModel();
}

std::optional<ProcessInfo> ProcessTableModel::processAt(int row) const
{
    if (row < 0 || row >= m_processes.size()) {
        return std::nullopt;
    }
    return m_processes.at(row);
}

std::optional<ProcessInfo> ProcessTableModel::processById(const QString &id) const
{
    return processAt(rowOfId(id));
}
