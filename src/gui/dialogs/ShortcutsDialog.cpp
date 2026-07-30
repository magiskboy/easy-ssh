// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ShortcutsDialog.h"

#include "core/settings/AppSettings.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

ShortcutsDialog::ShortcutsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(560, 480);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Action"), tr("Shortcut")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *resetButton = buttonBox->addButton(tr("Reset Defaults"), QDialogButtonBox::ResetRole);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ShortcutsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(resetButton, &QPushButton::clicked, this, &ShortcutsDialog::resetDefaults);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_tree, 1);
    root->addWidget(buttonBox);

    loadFromSettings();
    m_tree->expandAll();
}

void ShortcutsDialog::loadFromSettings()
{
    m_tree->clear();
    m_editors.clear();

    QHash<QString, QTreeWidgetItem *> groupItems;
    auto &settings = AppSettings::instance();

    for (const QString &actionId : AppSettings::shortcutActionIds()) {
        const QString group = AppSettings::shortcutGroup(actionId);
        QTreeWidgetItem *groupItem = groupItems.value(group);
        if (!groupItem) {
            groupItem = new QTreeWidgetItem(m_tree);
            groupItem->setText(0, group);
            groupItem->setFlags(Qt::ItemIsEnabled);
            groupItems.insert(group, groupItem);
        }

        auto *row = new QTreeWidgetItem(groupItem);
        row->setText(0, AppSettings::shortcutLabel(actionId));
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        auto *editor = new QKeySequenceEdit(settings.shortcut(actionId), m_tree);
        m_tree->setItemWidget(row, 1, editor);
        m_editors.insert(actionId, editor);
    }
}

void ShortcutsDialog::saveToSettings()
{
    auto &settings = AppSettings::instance();
    for (auto it = m_editors.cbegin(); it != m_editors.cend(); ++it) {
        settings.setShortcut(it.key(), it.value()->keySequence());
    }
}

void ShortcutsDialog::accept()
{
    saveToSettings();
    AppSettings::instance().notifyChanged();
    QDialog::accept();
}

void ShortcutsDialog::resetDefaults()
{
    AppSettings::instance().resetShortcutsToDefaults();
    for (auto it = m_editors.begin(); it != m_editors.end(); ++it) {
        it.value()->setKeySequence(AppSettings::defaultShortcut(it.key()));
    }
}
