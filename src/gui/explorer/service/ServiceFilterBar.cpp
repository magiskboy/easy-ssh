// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ServiceFilterBar.h"

#include "gui/explorer/service/ServiceTableModel.h"
#include "gui/widgets/ExplorerFilterProxy.h"
#include "gui/widgets/UiMetrics.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

ServiceFilterBar::ServiceFilterBar(ExplorerFilterProxy *proxy, QWidget *parent)
    : QWidget(parent), m_proxy(proxy)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(UiMetrics::tightSpacing);

    layout->addWidget(new QLabel(tr("Active:"), this));
    m_activeCombo = new QComboBox(this);
    m_activeCombo->addItem(tr("All"), QString());
    m_activeCombo->addItem(tr("Active"), QStringLiteral("Active"));
    m_activeCombo->addItem(tr("Inactive"), QStringLiteral("Inactive"));
    m_activeCombo->addItem(tr("Failed"), QStringLiteral("Failed"));
    m_activeCombo->addItem(tr("Activating"), QStringLiteral("Activating"));
    m_activeCombo->addItem(tr("Deactivating"), QStringLiteral("Deactivating"));
    m_activeCombo->addItem(tr("Unknown"), QStringLiteral("Unknown"));
    layout->addWidget(m_activeCombo);

    layout->addWidget(new QLabel(tr("Enabled:"), this));
    m_enabledCombo = new QComboBox(this);
    // Values must match EnabledColumn DisplayRole (raw unitFileState).
    m_enabledCombo->addItem(tr("All"), QString());
    m_enabledCombo->addItem(tr("enabled"), QStringLiteral("enabled"));
    m_enabledCombo->addItem(tr("disabled"), QStringLiteral("disabled"));
    m_enabledCombo->addItem(tr("static"), QStringLiteral("static"));
    m_enabledCombo->addItem(tr("masked"), QStringLiteral("masked"));
    m_enabledCombo->addItem(tr("indirect"), QStringLiteral("indirect"));
    m_enabledCombo->addItem(tr("generated"), QStringLiteral("generated"));
    m_enabledCombo->addItem(tr("transient"), QStringLiteral("transient"));
    m_enabledCombo->addItem(tr("alias"), QStringLiteral("alias"));
    layout->addWidget(m_enabledCombo);

    connect(m_activeCombo, &QComboBox::currentIndexChanged, this, &ServiceFilterBar::applyFilters);
    connect(m_enabledCombo, &QComboBox::currentIndexChanged, this, &ServiceFilterBar::applyFilters);
}

void ServiceFilterBar::applyFilters()
{
    if (!m_proxy) {
        return;
    }

    const QString active = m_activeCombo->currentData().toString();
    if (active.isEmpty()) {
        m_proxy->clearColumnFilter(ServiceTableModel::ActiveColumn);
    } else {
        m_proxy->setColumnFilter(ServiceTableModel::ActiveColumn, active);
    }

    const QString enabled = m_enabledCombo->currentData().toString();
    if (enabled.isEmpty()) {
        m_proxy->clearColumnFilter(ServiceTableModel::EnabledColumn);
    } else {
        m_proxy->setColumnFilter(ServiceTableModel::EnabledColumn, enabled);
    }
}
