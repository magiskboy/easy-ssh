// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "CategoryDialogShell.h"

#include "gui/widgets/UiMetrics.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QPalette>
#include <QSize>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace
{
constexpr int kPageIndexRole = Qt::UserRole;
constexpr int kCategoryIdRole = Qt::UserRole + 1;
} // namespace

CategoryDialogShell::CategoryDialogShell(QWidget *parent) : QWidget(parent)
{
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(false);
    m_tree->setIndentation(UiMetrics::sectionSpacing);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setFocusPolicy(Qt::StrongFocus);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setAllColumnsShowFocus(true);

    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("categorySidebar"));
    sidebar->setFrameShape(QFrame::StyledPanel);
    sidebar->setFrameShadow(QFrame::Plain);
    sidebar->setAutoFillBackground(true);
    sidebar->setFixedWidth(UiMetrics::categorySidebarWidth);
    {
        QPalette sidePalette = sidebar->palette();
        sidePalette.setColor(QPalette::Window, sidePalette.color(QPalette::Base));
        sidebar->setPalette(sidePalette);
        sidebar->setBackgroundRole(QPalette::Window);
    }
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(UiMetrics::chromeMargin,
                                   UiMetrics::chromeMargin,
                                   UiMetrics::chromeMargin,
                                   UiMetrics::chromeMargin);
    sideLayout->setSpacing(UiMetrics::chromeSpacing);
    sideLayout->addWidget(m_tree, 1);

    m_pages = new QStackedWidget(this);

    auto *content = new QFrame(this);
    content->setObjectName(QStringLiteral("categoryContent"));
    content->setFrameShape(QFrame::StyledPanel);
    content->setFrameShadow(QFrame::Plain);
    content->setAutoFillBackground(true);
    {
        QPalette contentPalette = content->palette();
        contentPalette.setColor(QPalette::Window, contentPalette.color(QPalette::Base));
        content->setPalette(contentPalette);
        content->setBackgroundRole(QPalette::Window);
    }
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(UiMetrics::chromeMargin,
                                      UiMetrics::chromeMargin,
                                      UiMetrics::chromeMargin,
                                      UiMetrics::chromeMargin);
    contentLayout->setSpacing(UiMetrics::chromeSpacing);
    contentLayout->addWidget(m_pages, 1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(UiMetrics::chromeMargin,
                               UiMetrics::chromeMargin,
                               UiMetrics::chromeMargin,
                               UiMetrics::chromeMargin);
    layout->setSpacing(UiMetrics::relatedSpacing);
    layout->addWidget(sidebar);
    layout->addWidget(content, 1);

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
    item->setSizeHint(0, QSize(0, UiMetrics::categoryRowHeight));
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
    item->setSizeHint(0, QSize(0, UiMetrics::categoryRowHeight));

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
