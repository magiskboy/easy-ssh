// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "WelcomeWidget.h"

#include "core/connection/Connection.h"
#include "core/settings/AppSettings.h"
#include "gui/models/ConnectionModel.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

WelcomeWidget::WelcomeWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignTop);

    auto *title = new QLabel(tr("Easy SSH"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 8);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    auto *hint = new QLabel(tr("Open a saved connection, or create one to get started."), this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    hint->setEnabled(false);

    auto *newButton = new QPushButton(tr("New Connection…"), this);
    newButton->setMinimumWidth(180);
    connect(newButton, &QPushButton::clicked, this, &WelcomeWidget::createConnectionRequested);

    auto *browseButton = new QPushButton(tr("Browse Connections"), this);
    browseButton->setFlat(true);
    connect(browseButton, &QPushButton::clicked, this, &WelcomeWidget::showConnectionsRequested);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setAlignment(Qt::AlignCenter);
    buttonRow->addWidget(newButton);
    buttonRow->addWidget(browseButton);

    m_recentHeading = new QLabel(tr("Recent"), this);
    QFont recentFont = m_recentHeading->font();
    recentFont.setBold(true);
    m_recentHeading->setFont(recentFont);

    m_recentList = new QListWidget(this);
    m_recentList->setAlternatingRowColors(true);
    m_recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recentList->setMaximumHeight(220);
    connect(m_recentList, &QListWidget::itemActivated, this, &WelcomeWidget::onRecentActivated);

    m_emptyRecentLabel = new QLabel(tr("No recent connections yet."), this);
    m_emptyRecentLabel->setAlignment(Qt::AlignCenter);
    m_emptyRecentLabel->setEnabled(false);

    layout->addStretch(1);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addSpacing(8);
    layout->addLayout(buttonRow);
    layout->addSpacing(16);
    layout->addWidget(m_recentHeading);
    layout->addWidget(m_recentList);
    layout->addWidget(m_emptyRecentLabel);
    layout->addStretch(2);

    rebuildRecentList();
}

void WelcomeWidget::setConnectionModel(ConnectionModel *model)
{
    m_model = model;
    rebuildRecentList();
}

void WelcomeWidget::refresh()
{
    rebuildRecentList();
}

void WelcomeWidget::onRecentActivated()
{
    openCurrentRecent();
}

void WelcomeWidget::openCurrentRecent()
{
    QListWidgetItem *item = m_recentList ? m_recentList->currentItem() : nullptr;
    if (!item) {
        return;
    }

    const QUuid id = item->data(Qt::UserRole).toUuid();
    if (id.isNull()) {
        return;
    }
    emit openConnectionRequested(id);
}

void WelcomeWidget::rebuildRecentList()
{
    if (!m_recentList) {
        return;
    }

    m_recentList->clear();
    const QList<QUuid> recentIds = AppSettings::instance().recentConnectionIds();
    int added = 0;
    for (const QUuid &id : recentIds) {
        if (!m_model) {
            break;
        }
        const auto connection = m_model->connectionById(id);
        if (!connection) {
            continue;
        }

        auto *item = new QListWidgetItem(connection->displayText(), m_recentList);
        item->setData(Qt::UserRole, id);
        item->setToolTip(connection->displayText());
        ++added;
    }

    const bool hasRecent = added > 0;
    m_recentList->setVisible(hasRecent);
    m_emptyRecentLabel->setVisible(!hasRecent);
    m_recentHeading->setVisible(true);
}
