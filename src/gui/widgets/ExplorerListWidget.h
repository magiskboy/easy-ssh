/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/widgets/ExplorerColumn.h"
#include "gui/widgets/IExplorerDetailFactory.h"

#include <QList>
#include <QModelIndex>
#include <QWidget>

#include <memory>

class ExplorerFilterProxy;
class QAbstractTableModel;
class QLabel;
class QLineEdit;
class QStackedLayout;
class QTableView;
class QToolButton;
class QVBoxLayout;

/// Reusable view-only multi-column explorer: search, sort, filter bar, detail dialog.
class ExplorerListWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerListWidget(QWidget *parent = nullptr);
    ~ExplorerListWidget() override;

    void setSourceModel(QAbstractTableModel *model);
    QAbstractTableModel *sourceModel() const { return m_sourceModel; }

    /// If unset before setSourceModel, a default ExplorerFilterProxy is created.
    void setFilterProxy(ExplorerFilterProxy *proxy);
    ExplorerFilterProxy *filterProxy() const { return m_proxy; }

    /// Reparents @p bar into the filter-bar host. Pass nullptr to clear.
    void setFilterBar(QWidget *bar);
    QWidget *filterBar() const { return m_filterBar; }

    void setDetailFactory(std::unique_ptr<IExplorerDetailFactory> factory);

    void setSearchPlaceholder(const QString &text);
    void setColumns(const QList<ExplorerColumn> &cols);

    void setRefreshVisible(bool visible);
    void setActivateOnSingleClick(bool enabled);

    void showEmptyState(const QString &message);
    void showLoading(const QString &message = {});
    void showList();

    /// Emits refreshRequested() for the domain layer to reload data.
    void refresh();

    QModelIndex currentSourceIndex() const;
    QTableView *tableView() const { return m_table; }
    QLineEdit *searchEdit() const { return m_searchEdit; }

signals:
    void refreshRequested();
    void currentSourceChanged(const QModelIndex &sourceIndex);

private slots:
    void onSearchTextChanged(const QString &text);
    void onActivated(const QModelIndex &proxyIndex);
    void onSelectionChanged();
    void openDetailForCurrent();

private:
    void ensureProxy();
    void rebindTableModel();
    void applyColumns();

    QAbstractTableModel *m_sourceModel = nullptr;
    ExplorerFilterProxy *m_proxy = nullptr;
    bool m_ownsProxy = false;
    std::unique_ptr<IExplorerDetailFactory> m_detailFactory;

    QLineEdit *m_searchEdit = nullptr;
    QToolButton *m_refreshButton = nullptr;
    QWidget *m_filterBarHost = nullptr;
    QVBoxLayout *m_filterBarLayout = nullptr;
    QWidget *m_filterBar = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QWidget *m_listHost = nullptr;
    QStackedLayout *m_stack = nullptr;

    QList<ExplorerColumn> m_columns;
    bool m_activateOnSingleClick = false;
};
