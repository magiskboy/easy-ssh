/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QTreeWidgetItem>
#include <QWidget>

class QStackedWidget;
class QTreeWidget;

/// Shared WinSCP-style sidebar tree + stacked content pages.
class CategoryDialogShell final : public QWidget
{
    Q_OBJECT

public:
    explicit CategoryDialogShell(QWidget *parent = nullptr);

    /// Parent category with no content page (click selects first child).
    QTreeWidgetItem *addGroup(const QString &title);

    /// Content page under @p parent (nullptr = root). Returns the tree item.
    QTreeWidgetItem *
    addPage(QTreeWidgetItem *parent, const QString &title, QWidget *page, const QString &id = {});

    void selectById(const QString &id);
    void selectFirst();
    void expandAll();

    QTreeWidget *categoryTree() const { return m_tree; }
    QStackedWidget *pages() const { return m_pages; }

private:
    void selectCategoryItem(QTreeWidgetItem *current);
    QTreeWidgetItem *findItemById(QTreeWidgetItem *parent, const QString &id) const;
    static int pageIndexOf(const QTreeWidgetItem *item);

    QTreeWidget *m_tree = nullptr;
    QStackedWidget *m_pages = nullptr;
};
