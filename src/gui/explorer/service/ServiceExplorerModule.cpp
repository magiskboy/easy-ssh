// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ServiceExplorerModule.h"

#include "core/explorer/service/ServiceInfo.h"
#include "core/explorer/service/ServiceSource.h"
#include "gui/explorer/service/ServiceDetailFactory.h"
#include "gui/explorer/service/ServiceFilterBar.h"
#include "gui/explorer/service/ServiceTableModel.h"
#include "gui/widgets/ExplorerFilterProxy.h"

#include <QCoreApplication>
#include <QObject>

QString ServiceExplorerModule::id() const
{
    return QStringLiteral("service");
}

QString ServiceExplorerModule::title() const
{
    return QCoreApplication::translate("ServiceExplorerModule", "Services");
}

QList<ExplorerColumn> ServiceExplorerModule::columns() const
{
    QList<ExplorerColumn> cols;
    cols.resize(ServiceTableModel::ColumnCount);

    cols[ServiceTableModel::UnitColumn].defaultWidth = 220;
    cols[ServiceTableModel::UnitColumn].defaultSort = true;
    cols[ServiceTableModel::UnitColumn].defaultSortOrder = Qt::AscendingOrder;
    cols[ServiceTableModel::ActiveColumn].defaultWidth = 100;
    cols[ServiceTableModel::SubColumn].defaultWidth = 96;
    cols[ServiceTableModel::EnabledColumn].defaultWidth = 96;
    cols[ServiceTableModel::DescriptionColumn].defaultWidth = 260;
    cols[ServiceTableModel::DescriptionColumn].stretch = true;
    cols[ServiceTableModel::PidColumn].defaultWidth = 72;

    return cols;
}

QList<int> ServiceExplorerModule::searchColumns() const
{
    return {ServiceTableModel::UnitColumn,
            ServiceTableModel::DescriptionColumn,
            ServiceTableModel::ActiveColumn,
            ServiceTableModel::EnabledColumn};
}

ExplorerTableModel *ServiceExplorerModule::createModel(QObject *parent)
{
    return new ServiceTableModel(parent);
}

IExplorerSource *ServiceExplorerModule::createSource(Session *session, QObject *parent)
{
    m_session = session;
    return new ServiceSource(session, parent);
}

QWidget *ServiceExplorerModule::createFilterBar(ExplorerFilterProxy *proxy, QWidget *parent)
{
    return new ServiceFilterBar(proxy, parent);
}

std::unique_ptr<IExplorerDetailFactory> ServiceExplorerModule::createDetailFactory()
{
    return std::make_unique<ServiceDetailFactory>(m_session);
}

void ServiceExplorerModule::connectSource(IExplorerSource *source,
                                          ExplorerTableModel *model,
                                          QObject *context)
{
    auto *serviceSource = qobject_cast<ServiceSource *>(source);
    auto *serviceModel = qobject_cast<ServiceTableModel *>(model);
    if (!serviceSource || !serviceModel || !context) {
        return;
    }
    QObject::connect(serviceSource,
                     &ServiceSource::snapshotReady,
                     context,
                     [serviceModel](const QVector<ServiceInfo> &services) {
                         serviceModel->applySnapshot(services);
                     });
}
