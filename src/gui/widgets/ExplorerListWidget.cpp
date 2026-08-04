// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ExplorerListWidget.h"

#include "gui/dialogs/ModelessDialog.h"
#include "gui/widgets/ExplorerFilterProxy.h"
#include "gui/widgets/UiMetrics.h"

#include <QAbstractItemView>
#include <QAbstractTableModel>
#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QStackedLayout>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

ExplorerListWidget::ExplorerListWidget(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(UiMetrics::relatedSpacing,
                             UiMetrics::relatedSpacing,
                             UiMetrics::relatedSpacing,
                             UiMetrics::relatedSpacing);
    root->setSpacing(UiMetrics::relatedSpacing);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(UiMetrics::tightSpacing);

    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText(tr("Search…"));
    m_searchEdit->setClearButtonEnabled(true);
    toolbarLayout->addWidget(m_searchEdit, 1);

    m_refreshButton = new QToolButton(toolbar);
    m_refreshButton->setText(tr("Refresh"));
    m_refreshButton->setToolTip(tr("Refresh"));
    m_refreshButton->setAutoRaise(true);
    m_refreshButton->setVisible(false);
    toolbarLayout->addWidget(m_refreshButton);
    root->addWidget(toolbar);

    m_filterBarHost = new QWidget(this);
    m_filterBarLayout = new QVBoxLayout(m_filterBarHost);
    m_filterBarLayout->setContentsMargins(0, 0, 0, 0);
    m_filterBarLayout->setSpacing(0);
    m_filterBarHost->setVisible(false);
    root->addWidget(m_filterBarHost);

    auto *stackHost = new QWidget(this);
    m_stack = new QStackedLayout(stackHost);
    m_stack->setContentsMargins(0, 0, 0, 0);

    m_emptyLabel = new QLabel(tr("No items."), stackHost);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setEnabled(false);
    m_emptyLabel->setMargin(UiMetrics::overlayMargin);

    m_listHost = new QWidget(stackHost);
    auto *listLayout = new QVBoxLayout(m_listHost);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    m_table = new QTableView(m_listHost);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->setWordWrap(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionsClickable(true);
    {
        QFont headerFont = m_table->horizontalHeader()->font();
        headerFont.setBold(false);
        m_table->horizontalHeader()->setFont(headerFont);
    }
    listLayout->addWidget(m_table);

    m_stack->addWidget(m_emptyLabel);
    m_stack->addWidget(m_listHost);
    root->addWidget(stackHost, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ExplorerListWidget::onSearchTextChanged);
    connect(m_refreshButton, &QToolButton::clicked, this, &ExplorerListWidget::refresh);
    connect(m_table, &QTableView::activated, this, &ExplorerListWidget::onActivated);
    connect(m_table, &QTableView::clicked, this, [this](const QModelIndex &index) {
        if (m_activateOnSingleClick) {
            onActivated(index);
        }
    });

    showEmptyState(tr("No items."));
}

ExplorerListWidget::~ExplorerListWidget() = default;

void ExplorerListWidget::setSourceModel(QAbstractTableModel *model)
{
    if (m_sourceModel == model) {
        return;
    }

    m_sourceModel = model;
    ensureProxy();
    m_proxy->setSourceModel(m_sourceModel);
    rebindTableModel();
    applyColumns();

    if (m_sourceModel && m_sourceModel->rowCount() > 0) {
        showList();
    }
}

void ExplorerListWidget::setFilterProxy(ExplorerFilterProxy *proxy)
{
    if (m_proxy == proxy) {
        return;
    }

    if (m_proxy && m_ownsProxy) {
        delete m_proxy;
    }

    m_proxy = proxy;
    m_ownsProxy = false;

    if (m_proxy && m_sourceModel) {
        m_proxy->setSourceModel(m_sourceModel);
    }
    rebindTableModel();
    if (!m_searchEdit->text().isEmpty() && m_proxy) {
        m_proxy->setFilterText(m_searchEdit->text());
    }
}

void ExplorerListWidget::setFilterBar(QWidget *bar)
{
    if (m_filterBar == bar) {
        return;
    }

    if (m_filterBar) {
        m_filterBarLayout->removeWidget(m_filterBar);
        m_filterBar->deleteLater();
        m_filterBar = nullptr;
    }

    m_filterBar = bar;
    if (m_filterBar) {
        m_filterBar->setParent(m_filterBarHost);
        m_filterBarLayout->addWidget(m_filterBar);
        m_filterBarHost->setVisible(true);
    } else {
        m_filterBarHost->setVisible(false);
    }
}

void ExplorerListWidget::setDetailFactory(std::unique_ptr<IExplorerDetailFactory> factory)
{
    m_detailFactory = std::move(factory);
}

void ExplorerListWidget::setSearchPlaceholder(const QString &text)
{
    m_searchEdit->setPlaceholderText(text);
}

void ExplorerListWidget::setColumns(const QList<ExplorerColumn> &cols)
{
    m_columns = cols;
    applyColumns();
}

void ExplorerListWidget::setRefreshVisible(bool visible)
{
    m_refreshButton->setVisible(visible);
}

void ExplorerListWidget::setActivateOnSingleClick(bool enabled)
{
    m_activateOnSingleClick = enabled;
}

void ExplorerListWidget::showEmptyState(const QString &message)
{
    m_emptyLabel->setText(message);
    m_stack->setCurrentWidget(m_emptyLabel);
}

void ExplorerListWidget::showLoading(const QString &message)
{
    m_emptyLabel->setText(message.isEmpty() ? tr("Loading…") : message);
    m_stack->setCurrentWidget(m_emptyLabel);
}

void ExplorerListWidget::showList()
{
    m_stack->setCurrentWidget(m_listHost);
}

void ExplorerListWidget::refresh()
{
    emit refreshRequested();
}

QModelIndex ExplorerListWidget::currentSourceIndex() const
{
    if (!m_proxy || !m_table->selectionModel()) {
        return {};
    }
    const QModelIndex proxyIndex = m_table->selectionModel()->currentIndex();
    if (!proxyIndex.isValid()) {
        return {};
    }
    return m_proxy->mapToSource(proxyIndex.siblingAtColumn(0));
}

void ExplorerListWidget::onSearchTextChanged(const QString &text)
{
    if (m_proxy) {
        m_proxy->setFilterText(text);
    }
}

void ExplorerListWidget::onActivated(const QModelIndex &proxyIndex)
{
    if (!m_detailFactory || !m_proxy || !m_sourceModel || !proxyIndex.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex.siblingAtColumn(0));
    if (!sourceIndex.isValid()) {
        return;
    }

    QDialog *dialog = m_detailFactory->createDetailDialog(m_sourceModel, sourceIndex, window());
    if (!dialog) {
        return;
    }
    // exec() is always application-modal (Qt docs). Explorers use modeless details.
    configureModelessDialog(dialog);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void ExplorerListWidget::onSelectionChanged()
{
    emit currentSourceChanged(currentSourceIndex());
}

void ExplorerListWidget::openDetailForCurrent()
{
    if (!m_table->selectionModel()) {
        return;
    }
    onActivated(m_table->selectionModel()->currentIndex());
}

void ExplorerListWidget::ensureProxy()
{
    if (m_proxy) {
        return;
    }
    m_proxy = new ExplorerFilterProxy(this);
    m_ownsProxy = true;
}

void ExplorerListWidget::rebindTableModel()
{
    if (m_table->selectionModel()) {
        disconnect(m_table->selectionModel(), nullptr, this, nullptr);
    }

    m_table->setModel(m_proxy);

    if (m_table->selectionModel()) {
        connect(m_table->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                &ExplorerListWidget::onSelectionChanged);
        connect(m_table->selectionModel(),
                &QItemSelectionModel::currentChanged,
                this,
                [this](const QModelIndex &, const QModelIndex &) { onSelectionChanged(); });
    }
}

void ExplorerListWidget::applyColumns()
{
    if (!m_table->model() || m_columns.isEmpty()) {
        return;
    }

    QHeaderView *header = m_table->horizontalHeader();
    const int modelColumns = m_table->model()->columnCount();
    int defaultSortColumn = -1;
    Qt::SortOrder defaultSortOrder = Qt::AscendingOrder;

    for (int i = 0; i < m_columns.size() && i < modelColumns; ++i) {
        const ExplorerColumn &col = m_columns.at(i);
        header->setSectionResizeMode(i, col.stretch ? QHeaderView::Stretch : col.resizeMode);
        if (!col.stretch && col.defaultWidth > 0) {
            m_table->setColumnWidth(i, col.defaultWidth);
        }
        if (col.defaultSort) {
            defaultSortColumn = i;
            defaultSortOrder = col.defaultSortOrder;
        }
    }

    if (defaultSortColumn >= 0) {
        m_table->sortByColumn(defaultSortColumn, defaultSortOrder);
    }
}
