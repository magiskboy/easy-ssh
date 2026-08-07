// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ContainerExplorerModule.h"

#include "core/explorer/container/ContainerInfo.h"
#include "core/explorer/container/ContainerSource.h"
#include "core/session/SessionRemoteExec.h"
#include "gui/explorer/container/ContainerDetailFactory.h"
#include "gui/explorer/container/ContainerFilterBar.h"
#include "gui/explorer/container/ContainerTableModel.h"
#include "gui/widgets/ExplorerFilterProxy.h"

#include <QCoreApplication>
#include <QObject>

QString ContainerExplorerModule::id() const
{
    return QStringLiteral("container");
}

QString ContainerExplorerModule::title() const
{
    return QCoreApplication::translate("ContainerExplorerModule", "Containers");
}

QList<ExplorerColumn> ContainerExplorerModule::columns() const
{
    QList<ExplorerColumn> cols;
    cols.resize(ContainerTableModel::ColumnCount);

    cols[ContainerTableModel::RuntimeColumn].defaultWidth = 96;
    cols[ContainerTableModel::NameColumn].defaultWidth = 160;
    cols[ContainerTableModel::NameColumn].defaultSort = true;
    cols[ContainerTableModel::NameColumn].defaultSortOrder = Qt::AscendingOrder;
    cols[ContainerTableModel::ImageColumn].defaultWidth = 220;
    cols[ContainerTableModel::ImageColumn].stretch = true;
    cols[ContainerTableModel::StateColumn].defaultWidth = 88;
    cols[ContainerTableModel::CpuColumn].defaultWidth = 72;
    cols[ContainerTableModel::MemColumn].defaultWidth = 72;
    cols[ContainerTableModel::PidColumn].defaultWidth = 72;
    cols[ContainerTableModel::IdColumn].defaultWidth = 110;

    return cols;
}

QList<int> ContainerExplorerModule::searchColumns() const
{
    return {ContainerTableModel::RuntimeColumn,
            ContainerTableModel::NameColumn,
            ContainerTableModel::ImageColumn,
            ContainerTableModel::StateColumn,
            ContainerTableModel::IdColumn};
}

ExplorerTableModel *ContainerExplorerModule::createModel(QObject *parent)
{
    return new ContainerTableModel(parent);
}

IExplorerSource *ContainerExplorerModule::createSource(Session *session, QObject *parent)
{
    m_session = session;
    auto *exec = new SessionRemoteExec(session, parent);
    return new ContainerSource(exec, parent);
}

QWidget *ContainerExplorerModule::createFilterBar(ExplorerFilterProxy *proxy, QWidget *parent)
{
    return new ContainerFilterBar(proxy, parent);
}

std::unique_ptr<IExplorerDetailFactory> ContainerExplorerModule::createDetailFactory()
{
    return std::make_unique<ContainerDetailFactory>(m_session);
}

void ContainerExplorerModule::connectSource(IExplorerSource *source,
                                            ExplorerTableModel *model,
                                            QObject *context)
{
    auto *containerSource = qobject_cast<ContainerSource *>(source);
    auto *containerModel = qobject_cast<ContainerTableModel *>(model);
    if (!containerSource || !containerModel || !context) {
        return;
    }
    QObject::connect(containerSource,
                     &ContainerSource::snapshotReady,
                     context,
                     [containerModel](const QVector<ContainerInfo> &containers) {
                         containerModel->applySnapshot(containers);
                     });
}
