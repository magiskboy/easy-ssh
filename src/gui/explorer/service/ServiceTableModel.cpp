// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ServiceTableModel.h"

#include "core/explorer/service/ServiceParser.h"

#include <QHash>

ServiceTableModel::ServiceTableModel(QObject *parent) : ExplorerTableModel(parent) {}

int ServiceTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_services.size();
}

int ServiceTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant ServiceTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_services.size()) {
        return {};
    }

    const ServiceInfo &info = m_services.at(index.row());
    if (role == IdRole) {
        return info.id();
    }
    if (role == Qt::DisplayRole) {
        return displayValue(info, index.column());
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == UnitColumn) {
            return info.unit;
        }
        if (index.column() == DescriptionColumn) {
            return info.description;
        }
        return displayValue(info, index.column());
    }
    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case PidColumn:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (role == SortValueRole) {
        switch (index.column()) {
        case PidColumn:
            return info.mainPid;
        case ActiveColumn:
            return ServiceParser::formatActiveStateDisplay(info.activeState);
        default:
            return displayValue(info, index.column());
        }
    }
    return {};
}

QVariant ServiceTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case UnitColumn:
        return tr("Unit");
    case ActiveColumn:
        return tr("Active");
    case SubColumn:
        return tr("Sub");
    case EnabledColumn:
        return tr("Enabled");
    case DescriptionColumn:
        return tr("Description");
    case PidColumn:
        return tr("PID");
    default:
        return {};
    }
}

QString ServiceTableModel::rowId(int row) const
{
    if (row < 0 || row >= m_services.size()) {
        return {};
    }
    return m_services.at(row).id();
}

QVariant ServiceTableModel::displayValue(const ServiceInfo &info, int column)
{
    switch (column) {
    case UnitColumn:
        return info.unit;
    case ActiveColumn:
        return ServiceParser::formatActiveStateDisplay(info.activeState);
    case SubColumn:
        return info.subState.isEmpty() ? QStringLiteral("—") : info.subState;
    case EnabledColumn:
        return info.unitFileState.isEmpty() ? QStringLiteral("—") : info.unitFileState;
    case DescriptionColumn:
        return info.description.isEmpty() ? QStringLiteral("—") : info.description;
    case PidColumn:
        return ServiceParser::formatPidDisplay(info.mainPid);
    default:
        return {};
    }
}

namespace
{
bool serviceEquals(const ServiceInfo &a, const ServiceInfo &b)
{
    return a.manager == b.manager && a.unit == b.unit && a.description == b.description &&
           a.loadState == b.loadState && a.activeState == b.activeState &&
           a.subState == b.subState && a.unitFileState == b.unitFileState && a.mainPid == b.mainPid;
}
} // namespace

void ServiceTableModel::applySnapshot(const QVector<ServiceInfo> &services)
{
    QHash<QString, ServiceInfo> incoming;
    incoming.reserve(services.size());
    QStringList order;
    order.reserve(services.size());
    for (const ServiceInfo &info : services) {
        const QString id = info.id();
        if (id.isEmpty() || incoming.contains(id)) {
            continue;
        }
        incoming.insert(id, info);
        order.append(id);
    }

    for (int row = m_services.size() - 1; row >= 0; --row) {
        if (!incoming.contains(m_services.at(row).id())) {
            beginRemoveIdRows(row, row);
            m_services.removeAt(row);
            endRemoveIdRows();
        }
    }

    QVector<ServiceInfo> toInsert;
    toInsert.reserve(order.size());
    for (const QString &id : order) {
        const ServiceInfo &info = incoming.value(id);
        const int row = rowOfId(id);
        if (row >= 0) {
            if (!serviceEquals(m_services.at(row), info)) {
                m_services[row] = info;
                emitRowDataChanged(row, ColumnCount);
            }
        } else {
            toInsert.append(info);
        }
    }

    if (!toInsert.isEmpty()) {
        const int first = m_services.size();
        const int last = first + toInsert.size() - 1;
        beginInsertIdRows(first, last);
        m_services += toInsert;
        endInsertIdRows();
    } else {
        rebuildIdIndex();
    }
}

void ServiceTableModel::clear()
{
    if (m_services.isEmpty()) {
        return;
    }
    beginResetModel();
    m_services.clear();
    clearIdIndex();
    endResetModel();
}

std::optional<ServiceInfo> ServiceTableModel::serviceAt(int row) const
{
    if (row < 0 || row >= m_services.size()) {
        return std::nullopt;
    }
    return m_services.at(row);
}

std::optional<ServiceInfo> ServiceTableModel::serviceById(const QString &id) const
{
    return serviceAt(rowOfId(id));
}
