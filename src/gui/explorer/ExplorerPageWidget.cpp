// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ExplorerPageWidget.h"

#include "core/explorer/ExplorerTypes.h"
#include "core/explorer/IExplorerSource.h"
#include "core/session/Session.h"
#include "gui/explorer/ExplorerTableModel.h"
#include "gui/explorer/IExplorerModule.h"
#include "gui/widgets/ExplorerFilterProxy.h"
#include "gui/widgets/ExplorerListWidget.h"

#include <QAbstractItemModel>
#include <QVBoxLayout>

ExplorerPageWidget::ExplorerPageWidget(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_list = new ExplorerListWidget(this);
    m_list->setRefreshVisible(true);
    root->addWidget(m_list, 1);

    connect(m_list,
            &ExplorerListWidget::refreshRequested,
            this,
            &ExplorerPageWidget::onRefreshRequested);
}

ExplorerPageWidget::~ExplorerPageWidget()
{
    unbind();
}

void ExplorerPageWidget::bind(std::unique_ptr<IExplorerModule> module, Session *session)
{
    unbind();
    if (!module || !session) {
        return;
    }

    m_module = std::move(module);
    m_session = session;

    m_model = m_module->createModel(this);
    m_source = m_module->createSource(session, this);

    m_list->setSearchPlaceholder(tr("Search %1…").arg(m_module->title().toLower()));
    m_list->setColumns(m_module->columns());
    m_list->setDetailFactory(m_module->createDetailFactory());
    m_list->setSourceModel(m_model);

    if (ExplorerFilterProxy *proxy = m_list->filterProxy()) {
        proxy->setSearchColumns(m_module->searchColumns());
        proxy->setSortRole(Qt::UserRole + 10); // SortValueRole used by process/container models
    }
    m_list->setFilterBar(m_module->createFilterBar(m_list->filterProxy(), m_list));

    connect(m_source,
            &IExplorerSource::capabilityChanged,
            this,
            &ExplorerPageWidget::onCapabilityChanged);
    connect(m_source, &IExplorerSource::failed, this, &ExplorerPageWidget::onFailed);

    m_module->connectSource(m_source, m_model, this);
    connect(
        m_model, &QAbstractItemModel::rowsInserted, this, &ExplorerPageWidget::onCapabilityChanged);
    connect(
        m_model, &QAbstractItemModel::rowsRemoved, this, &ExplorerPageWidget::onCapabilityChanged);
    connect(
        m_model, &QAbstractItemModel::modelReset, this, &ExplorerPageWidget::onCapabilityChanged);

    applyCapabilityUi();
    m_source->start();
}

void ExplorerPageWidget::unbind()
{
    stopSource();
    if (m_list) {
        m_list->setSourceModel(nullptr);
        m_list->setFilterBar(nullptr);
        m_list->setDetailFactory(nullptr);
    }
    if (m_source) {
        delete m_source;
        m_source = nullptr;
    }
    if (m_model) {
        delete m_model;
        m_model = nullptr;
    }
    m_module.reset();
    m_session.clear();
}

void ExplorerPageWidget::onCapabilityChanged()
{
    applyCapabilityUi();
}

void ExplorerPageWidget::onFailed(const QString &error)
{
    if (m_list && m_source && m_source->capability() != ExplorerCapability::Available) {
        m_list->showEmptyState(error);
    }
}

void ExplorerPageWidget::onRefreshRequested()
{
    if (m_source) {
        m_source->refresh();
    }
}

void ExplorerPageWidget::applyCapabilityUi()
{
    if (!m_list || !m_source) {
        return;
    }

    switch (m_source->capability()) {
    case ExplorerCapability::Checking:
        m_list->showLoading(m_source->capabilityMessage().isEmpty()
                                ? tr("Checking…")
                                : m_source->capabilityMessage());
        break;
    case ExplorerCapability::Unavailable:
    case ExplorerCapability::PermissionDenied:
    case ExplorerCapability::Error:
        m_list->showEmptyState(m_source->capabilityMessage().isEmpty()
                                   ? tr("Unavailable")
                                   : m_source->capabilityMessage());
        break;
    case ExplorerCapability::Available:
        if (m_model && m_model->rowCount() > 0) {
            m_list->showList();
        } else {
            const QString title = m_module ? m_module->title().toLower() : tr("items");
            m_list->showEmptyState(tr("No %1.").arg(title));
        }
        break;
    }
}

void ExplorerPageWidget::stopSource()
{
    if (m_source) {
        m_source->stop();
    }
}
