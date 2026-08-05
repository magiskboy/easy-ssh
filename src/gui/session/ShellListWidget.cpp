// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ShellListWidget.h"

#include "ShellDockHost.h"
#include "core/session/Session.h"
#include "gui/widgets/UiHelpers.h"

#include <QAbstractItemView>
#include <QDrag>
#include <QFrame>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace
{
class ShellDragListWidget final : public QListWidget
{
public:
    explicit ShellDragListWidget(QWidget *parent = nullptr) : QListWidget(parent)
    {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::CopyAction);
    }

protected:
    void startDrag(Qt::DropActions supportedActions) override
    {
        QListWidgetItem *item = currentItem();
        if (!item) {
            return;
        }
        const QUuid id = item->data(Qt::UserRole).toUuid();
        if (id.isNull()) {
            return;
        }
        auto *mime = new QMimeData;
        mime->setData(QLatin1String(ShellDockHost::kShellMimeType),
                      id.toString(QUuid::WithoutBraces).toUtf8());
        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->exec(supportedActions, Qt::CopyAction);
    }
};
} // namespace

ShellListWidget::ShellListWidget(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_list = new ShellDragListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setStyleSheet(QStringLiteral("QListWidget {"
                                         "  background: transparent;"
                                         "  border: none;"
                                         "  outline: none;"
                                         "  padding: 0;"
                                         "}"
                                         "QListWidget::item {"
                                         "  padding: 2px 8px 2px 20px;"
                                         "}"
                                         "QListWidget::item:selected {"
                                         "  background: palette(highlight);"
                                         "  color: palette(highlighted-text);"
                                         "}"
                                         "QListWidget::item:hover:!selected {"
                                         "  background: palette(mid);"
                                         "}"));
    connect(m_list, &QListWidget::itemClicked, this, &ShellListWidget::onItemClicked);
    connect(m_list, &QWidget::customContextMenuRequested, this, &ShellListWidget::onContextMenu);
    root->addWidget(m_list, 1);
}

void ShellListWidget::bindSession(Session *session)
{
    if (m_session == session) {
        return;
    }
    unbindSession();
    m_session = session;
    if (!m_session) {
        refresh();
        return;
    }
    connect(m_session, &Session::shellsChanged, this, &ShellListWidget::refresh);
    connect(m_session, &Session::activeShellChanged, this, &ShellListWidget::refresh);
    connect(m_session, &Session::stateChanged, this, &ShellListWidget::refresh);
    refresh();
}

void ShellListWidget::unbindSession()
{
    if (m_session) {
        disconnect(m_session, nullptr, this, nullptr);
        m_session = nullptr;
    }
    refresh();
}

void ShellListWidget::refresh()
{
    m_list->clear();
    if (!m_session) {
        return;
    }
    const QUuid active = m_session->activeShellId();
    for (const ShellChannelState &shell : m_session->shells()) {
        if (shell.auxiliary) {
            continue;
        }
        auto *item = new QListWidgetItem(shell.title, m_list);
        item->setData(Qt::UserRole, shell.id);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
        if (shell.id == active) {
            item->setSelected(true);
            m_list->setCurrentItem(item);
        }
        if (shell.state == ChannelState::Closed || shell.state == ChannelState::Failed) {
            item->setForeground(Qt::gray);
        }
    }
}

void ShellListWidget::onItemClicked()
{
    if (!m_session || !m_list->currentItem()) {
        return;
    }
    const QUuid id = m_list->currentItem()->data(Qt::UserRole).toUuid();
    emit shellActivationRequested(id);
}

void ShellListWidget::onContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.addAction(tr("New shell"), this, &ShellListWidget::newShell);
    if (m_list->itemAt(pos)) {
        menu.addAction(tr("Rename…"), this, &ShellListWidget::renameSelected);
        menu.addAction(tr("Close shell"), this, &ShellListWidget::closeSelected);
    }
    menu.exec(m_list->mapToGlobal(pos));
}

void ShellListWidget::newShell()
{
    if (!m_session) {
        return;
    }
    m_session->newShell();
}

void ShellListWidget::renameSelected()
{
    if (!m_session || !m_list->currentItem()) {
        return;
    }
    const QUuid id = m_list->currentItem()->data(Qt::UserRole).toUuid();
    bool ok = false;
    const QString name = UiHelpers::getText(
        this, {tr("Rename Shell"), tr("Name:"), m_list->currentItem()->text()}, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_session->renameShell(id, name.trimmed());
    }
}

void ShellListWidget::closeSelected()
{
    if (!m_session || !m_list->currentItem()) {
        return;
    }
    m_session->closeShell(m_list->currentItem()->data(Qt::UserRole).toUuid());
}
