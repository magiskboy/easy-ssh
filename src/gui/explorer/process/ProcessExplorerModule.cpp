// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ProcessExplorerModule.h"

#include "core/explorer/process/ProcessSource.h"
#include "gui/explorer/process/ProcessDetailFactory.h"
#include "gui/explorer/process/ProcessTableModel.h"
#include "gui/widgets/ExplorerFilterProxy.h"

#include <QCoreApplication>

QString ProcessExplorerModule::id() const
{
    return QStringLiteral("process");
}

QString ProcessExplorerModule::title() const
{
    return QCoreApplication::translate("ProcessExplorerModule", "Processes");
}

QList<ExplorerColumn> ProcessExplorerModule::columns() const
{
    QList<ExplorerColumn> cols;
    cols.resize(ProcessTableModel::ColumnCount);

    cols[ProcessTableModel::PidColumn].defaultWidth = 72;
    cols[ProcessTableModel::PidColumn].defaultSort = true;
    cols[ProcessTableModel::PidColumn].defaultSortOrder = Qt::AscendingOrder;

    cols[ProcessTableModel::UserColumn].defaultWidth = 96;
    cols[ProcessTableModel::CpuColumn].defaultWidth = 64;
    cols[ProcessTableModel::MemColumn].defaultWidth = 64;
    cols[ProcessTableModel::StateColumn].defaultWidth = 56;
    cols[ProcessTableModel::CommandColumn].stretch = true;

    return cols;
}

QList<int> ProcessExplorerModule::searchColumns() const
{
    return {ProcessTableModel::PidColumn,
            ProcessTableModel::UserColumn,
            ProcessTableModel::StateColumn,
            ProcessTableModel::CommandColumn};
}

ExplorerTableModel *ProcessExplorerModule::createModel(QObject *parent)
{
    return new ProcessTableModel(parent);
}

IExplorerSource *ProcessExplorerModule::createSource(Session *session, QObject *parent)
{
    return new ProcessSource(session, parent);
}

QWidget *ProcessExplorerModule::createFilterBar(ExplorerFilterProxy * /*proxy*/,
                                                QWidget * /*parent*/)
{
    return nullptr;
}

std::unique_ptr<IExplorerDetailFactory> ProcessExplorerModule::createDetailFactory()
{
    return std::make_unique<ProcessDetailFactory>();
}
