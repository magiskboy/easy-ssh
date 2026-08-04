// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ContainerFilterBar.h"

#include "gui/explorer/container/ContainerTableModel.h"
#include "gui/widgets/ExplorerFilterProxy.h"
#include "gui/widgets/UiMetrics.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

ContainerFilterBar::ContainerFilterBar(ExplorerFilterProxy *proxy, QWidget *parent)
    : QWidget(parent), m_proxy(proxy)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::tightSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::tightSpacing);
    layout->setSpacing(UiMetrics::relatedSpacing);

    layout->addWidget(new QLabel(tr("Runtime:"), this));
    m_runtimeCombo = new QComboBox(this);
    m_runtimeCombo->addItem(tr("All"), QString());
    m_runtimeCombo->addItem(QStringLiteral("podman"), QStringLiteral("podman"));
    m_runtimeCombo->addItem(QStringLiteral("docker"), QStringLiteral("docker"));
    m_runtimeCombo->addItem(QStringLiteral("containerd"), QStringLiteral("containerd"));
    layout->addWidget(m_runtimeCombo);

    layout->addWidget(new QLabel(tr("State:"), this));
    m_stateCombo = new QComboBox(this);
    m_stateCombo->addItem(tr("All"), QString());
    m_stateCombo->addItem(tr("Running"), QStringLiteral("Running"));
    m_stateCombo->addItem(tr("Exited"), QStringLiteral("Exited"));
    m_stateCombo->addItem(tr("Created"), QStringLiteral("Created"));
    m_stateCombo->addItem(tr("Paused"), QStringLiteral("Paused"));
    m_stateCombo->addItem(tr("Unknown"), QStringLiteral("Unknown"));
    layout->addWidget(m_stateCombo);

    layout->addStretch(1);

    connect(
        m_runtimeCombo, &QComboBox::currentIndexChanged, this, &ContainerFilterBar::applyFilters);
    connect(m_stateCombo, &QComboBox::currentIndexChanged, this, &ContainerFilterBar::applyFilters);
}

void ContainerFilterBar::applyFilters()
{
    if (!m_proxy) {
        return;
    }

    const QString runtime = m_runtimeCombo->currentData().toString();
    if (runtime.isEmpty()) {
        m_proxy->clearColumnFilter(ContainerTableModel::RuntimeColumn);
    } else {
        m_proxy->setColumnFilter(ContainerTableModel::RuntimeColumn, runtime);
    }

    const QString state = m_stateCombo->currentData().toString();
    if (state.isEmpty()) {
        m_proxy->clearColumnFilter(ContainerTableModel::StateColumn);
    } else {
        m_proxy->setColumnFilter(ContainerTableModel::StateColumn, state);
    }
}
