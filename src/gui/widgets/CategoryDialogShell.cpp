// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "CategoryDialogShell.h"

#include <QHBoxLayout>
#include <QSize>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace
{
constexpr int kRowHeight = 28;
constexpr int kPageIndexRole = Qt::UserRole;
constexpr int kCategoryIdRole = Qt::UserRole + 1;
} // namespace

CategoryDialogShell::CategoryDialogShell(QWidget *parent) : QWidget(parent)
{
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setFixedWidth(160);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(false);
    m_tree->setIndentation(12);
    m_tree->setStyleSheet(QStringLiteral("QTreeWidget {"
                                         "  border: none;"
                                         "  background: transparent;"
                                         "  padding: 2px 0 2px 0;"
                                         "  outline: none;"
                                         "}"
                                         "QTreeWidget::item {"
                                         "  padding-top: 4px;"
                                         "  padding-bottom: 4px;"
                                         "  padding-left: 2px;"
                                         "  padding-right: 4px;"
                                         "  border-radius: 3px;"
                                         "}"
                                         "QTreeWidget::branch {"
                                         "  border-image: none;"
                                         "  background: transparent;"
                                         "}"));

    m_pages = new QStackedWidget(this);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(m_tree);
    layout->addWidget(m_pages, 1);

    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current) {
        selectCategoryItem(current);
    });
}

QTreeWidgetItem *CategoryDialogShell::addGroup(const QString &title)
{
    auto *item = new QTreeWidgetItem(m_tree);
    item->setText(0, title);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(0, kPageIndexRole, -1);
    item->setSizeHint(0, QSize(0, kRowHeight));
    return item;
}

QTreeWidgetItem *CategoryDialogShell::addPage(QTreeWidgetItem *parent,
                                              const QString &title,
                                              QWidget *page,
                                              const QString &id)
{
    QTreeWidgetItem *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
    item->setText(0, title);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setSizeHint(0, QSize(0, kRowHeight));

    const int index = m_pages->addWidget(page);
    item->setData(0, kPageIndexRole, index);
    if (!id.isEmpty()) {
        item->setData(0, kCategoryIdRole, id);
    }
    return item;
}

void CategoryDialogShell::selectById(const QString &id)
{
    if (id.isEmpty()) {
        return;
    }
    QTreeWidgetItem *item = findItemById(m_tree->invisibleRootItem(), id);
    if (item) {
        m_tree->setCurrentItem(item);
    }
}

void CategoryDialogShell::selectFirst()
{
    if (m_tree->topLevelItemCount() == 0) {
        return;
    }
    QTreeWidgetItem *first = m_tree->topLevelItem(0);
    if (pageIndexOf(first) < 0 && first->childCount() > 0) {
        first = first->child(0);
    }
    m_tree->setCurrentItem(first);
}

void CategoryDialogShell::expandAll()
{
    m_tree->expandAll();
}

void CategoryDialogShell::selectCategoryItem(QTreeWidgetItem *current)
{
    if (!current) {
        return;
    }

    const int index = pageIndexOf(current);
    if (index >= 0) {
        m_pages->setCurrentIndex(index);
        return;
    }

    if (current->childCount() > 0) {
        QTreeWidgetItem *child = current->child(0);
        // Prefer first child that has a page.
        for (int i = 0; i < current->childCount(); ++i) {
            if (pageIndexOf(current->child(i)) >= 0) {
                child = current->child(i);
                break;
            }
        }
        m_tree->setCurrentItem(child);
    }
}

QTreeWidgetItem *CategoryDialogShell::findItemById(QTreeWidgetItem *parent, const QString &id) const
{
    if (!parent) {
        return nullptr;
    }
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->data(0, kCategoryIdRole).toString() == id) {
            return child;
        }
        if (QTreeWidgetItem *found = findItemById(child, id)) {
            return found;
        }
    }
    return nullptr;
}

int CategoryDialogShell::pageIndexOf(const QTreeWidgetItem *item)
{
    if (!item) {
        return -1;
    }
    const QVariant value = item->data(0, kPageIndexRole);
    if (!value.isValid()) {
        return -1;
    }
    return value.toInt();
}
