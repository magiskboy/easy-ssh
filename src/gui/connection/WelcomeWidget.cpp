// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "WelcomeWidget.h"

#include "core/connection/Connection.h"
#include "core/settings/AppSettings.h"
#include "gui/models/ConnectionModel.h"
#include "gui/widgets/UiMetrics.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

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
    m_titleLabel = title;

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

    m_contentColumn = new QWidget(this);
    m_contentColumn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto *contentLayout = new QVBoxLayout(m_contentColumn);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    m_recentHeading = new QLabel(tr("Recent"), m_contentColumn);
    QFont recentFont = m_recentHeading->font();
    m_recentHeading->setFont(recentFont);

    m_recentList = new QListWidget(m_contentColumn);
    m_recentList->setAlternatingRowColors(true);
    m_recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recentList->setMaximumHeight(220);
    m_recentList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recentList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    connect(m_recentList, &QListWidget::itemActivated, this, &WelcomeWidget::onRecentActivated);

    m_emptyRecentLabel = new QLabel(tr("No recent connections yet."), m_contentColumn);
    m_emptyRecentLabel->setAlignment(Qt::AlignCenter);
    m_emptyRecentLabel->setEnabled(false);

    m_shortcutsHeading = new QLabel(tr("Keyboard"), m_contentColumn);
    QFont shortcutsFont = m_shortcutsHeading->font();
    m_shortcutsHeading->setFont(shortcutsFont);

    m_shortcutsGrid = new QGridLayout();
    m_shortcutsGrid->setHorizontalSpacing(24);
    m_shortcutsGrid->setVerticalSpacing(4);
    m_shortcutsGrid->setColumnStretch(0, 1);

    contentLayout->addWidget(m_recentHeading);
    contentLayout->addWidget(m_recentList);
    contentLayout->addWidget(m_emptyRecentLabel);
    contentLayout->addSpacing(4);
    contentLayout->addWidget(m_shortcutsHeading);
    contentLayout->addLayout(m_shortcutsGrid);

    layout->addStretch(1);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addSpacing(8);
    layout->addLayout(buttonRow);
    layout->addSpacing(16);
    layout->addWidget(m_contentColumn, 0, Qt::AlignHCenter);
    layout->addStretch(2);

    connect(&AppSettings::instance(),
            &AppSettings::settingsChanged,
            this,
            &WelcomeWidget::refreshShortcutHints);

    rebuildRecentList();
    refreshShortcutHints();
    updateContentColumnWidth();
}

void WelcomeWidget::setConnectionModel(ConnectionModel *model)
{
    m_model = model;
    rebuildRecentList();
}

void WelcomeWidget::refresh()
{
    rebuildRecentList();
    refreshShortcutHints();
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
    updateContentColumnWidth();
}

void WelcomeWidget::addShortcutHintRow(int row, QStringView actionId, const QString &fallbackLabel)
{
    const QString actionIdStr = actionId.toString();
    QString label = AppSettings::shortcutLabel(actionIdStr);
    if (label.isEmpty()) {
        label = fallbackLabel;
    }
    auto *nameLabel = new QLabel(label, m_contentColumn);
    nameLabel->setEnabled(false);

    const QString keys =
        AppSettings::instance().shortcut(actionIdStr).toString(QKeySequence::NativeText);
    auto *keysLabel = new QLabel(keys, m_contentColumn);
    keysLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_shortcutsGrid->addWidget(nameLabel, row, 0);
    m_shortcutsGrid->addWidget(keysLabel, row, 1);
}

void WelcomeWidget::refreshShortcutHints()
{
    if (!m_shortcutsGrid) {
        return;
    }

    while (QLayoutItem *item = m_shortcutsGrid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    addShortcutHintRow(0, QStringLiteral("general.quickConnect"), tr("Quick Connect"));
    addShortcutHintRow(1, QStringLiteral("general.newConnection"), tr("New Connection"));
    addShortcutHintRow(2, QStringLiteral("general.commandPalette"), tr("Command Palette"));
    addShortcutHintRow(3, QStringLiteral("general.settings"), tr("Settings"));
    updateContentColumnWidth();
}

void WelcomeWidget::updateContentColumnWidth()
{
    if (!m_contentColumn || !m_titleLabel) {
        return;
    }

    const QFontMetrics titleFm(m_titleLabel->font());
    const int titleBased = titleFm.horizontalAdvance(m_titleLabel->text()) * 4;

    int contentHint = 0;
    if (m_recentList && m_recentList->isVisible()) {
        const QFontMetrics listFm(m_recentList->font());
        const int frame = m_recentList->frameWidth() * 2 + 24;
        for (int i = 0; i < m_recentList->count(); ++i) {
            const QListWidgetItem *item = m_recentList->item(i);
            if (!item) {
                continue;
            }
            contentHint = std::max(contentHint, listFm.horizontalAdvance(item->text()) + frame);
        }
    }

    if (m_shortcutsGrid) {
        const QFontMetrics shortcutFm(font());
        for (int row = 0; row < m_shortcutsGrid->rowCount(); ++row) {
            int rowWidth = m_shortcutsGrid->horizontalSpacing();
            for (int col = 0; col < m_shortcutsGrid->columnCount(); ++col) {
                QLayoutItem *item = m_shortcutsGrid->itemAtPosition(row, col);
                if (!item || !item->widget()) {
                    continue;
                }
                if (auto *label = qobject_cast<QLabel *>(item->widget())) {
                    rowWidth += shortcutFm.horizontalAdvance(label->text());
                }
            }
            contentHint = std::max(contentHint, rowWidth);
        }
    }

    const int width = std::clamp(std::max(titleBased, contentHint),
                                 UiMetrics::welcomeContentMinWidth,
                                 UiMetrics::welcomeContentMaxWidth);
    m_contentColumn->setFixedWidth(width);
}
