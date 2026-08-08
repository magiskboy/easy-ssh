// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "CommandPaletteDialog.h"

#include "core/util/FuzzyMatch.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QShowEvent>
#include <QVBoxLayout>
#include <algorithm>

namespace
{

constexpr int kKindRole = Qt::UserRole;
constexpr int kActionIdRole = Qt::UserRole + 1;
constexpr int kConnectionIdRole = Qt::UserRole + 2;
constexpr int kTerminalIdRole = Qt::UserRole + 3;
constexpr int kEnabledRole = Qt::UserRole + 4;

} // namespace

CommandPaletteDialog::CommandPaletteDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
    setFixedWidth(520);
    setMinimumHeight(320);
    setMaximumHeight(480);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setPlaceholderText(tr("Filter…"));
    m_filterEdit->installEventFilter(this);

    m_list = new QListWidget(this);
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->installEventFilter(this);

    m_hintLabel = new QLabel(tr("↑↓ navigate · Enter select · Esc close"), this);
    m_hintLabel->setEnabled(false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 10);
    root->setSpacing(8);
    root->addWidget(m_filterEdit);
    root->addWidget(m_list, 1);
    root->addWidget(m_hintLabel);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::rebuildVisibleList);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        activateCurrentItem();
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *) {
        activateCurrentItem();
    });
}

void CommandPaletteDialog::setActionItems(const QList<ActionItem> &items)
{
    m_actions = items;
}

void CommandPaletteDialog::setConnectionItems(const QList<ConnectionItem> &items)
{
    m_connections = items;
}

void CommandPaletteDialog::setTerminalItems(const QList<TerminalItem> &items)
{
    m_terminals = items;
}

void CommandPaletteDialog::openMode(Mode mode)
{
    m_mode = mode;
    switch (mode) {
    case Mode::Actions:
        setWindowTitle(tr("Command Palette"));
        m_filterEdit->setPlaceholderText(tr("Filter actions…"));
        break;
    case Mode::Connections:
        setWindowTitle(tr("Quick Connect"));
        m_filterEdit->setPlaceholderText(tr("Search connections or create…"));
        break;
    case Mode::Terminals:
        setWindowTitle(tr("Go to Terminal"));
        m_filterEdit->setPlaceholderText(tr("Search open terminals…"));
        break;
    }

    m_filterEdit->clear();
    rebuildVisibleList();
    centerOnParent();
    show();
    raise();
    activateWindow();
    m_filterEdit->setFocus(Qt::PopupFocusReason);
    m_filterEdit->selectAll();
}

void CommandPaletteDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    centerOnParent();
}

void CommandPaletteDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}

bool CommandPaletteDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress) {
        return QDialog::eventFilter(watched, event);
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (watched == m_filterEdit) {
        switch (keyEvent->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            QApplication::sendEvent(m_list, keyEvent);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activateCurrentItem();
            return true;
        case Qt::Key_Escape:
            reject();
            return true;
        default:
            break;
        }
    } else if (watched == m_list) {
        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activateCurrentItem();
            return true;
        case Qt::Key_Escape:
            reject();
            return true;
        default:
            if (!keyEvent->text().isEmpty() && keyEvent->modifiers() == Qt::NoModifier) {
                m_filterEdit->setFocus(Qt::OtherFocusReason);
                QApplication::sendEvent(m_filterEdit, keyEvent);
                return true;
            }
            break;
        }
    }

    return QDialog::eventFilter(watched, event);
}

void CommandPaletteDialog::centerOnParent()
{
    QWidget *anchor = parentWidget();
    if (!anchor) {
        return;
    }
    const QRect geo = anchor->window()->geometry();
    const QSize sz = sizeHint().expandedTo(QSize(520, 360));
    resize(sz.width(), qMin(sz.height(), maximumHeight()));
    move(geo.center() - rect().center());
}

QListWidgetItem *CommandPaletteDialog::makeRow(const BuiltItem &item)
{
    QString text = item.primary;
    if (!item.secondary.isEmpty()) {
        text += QLatin1Char('\n') + item.secondary;
    }
    if (!item.shortcutText.isEmpty()) {
        text += QLatin1String("    ") + item.shortcutText;
    }

    auto *row = new QListWidgetItem(text);
    row->setData(kKindRole, static_cast<int>(item.kind));
    row->setData(kActionIdRole, item.actionId);
    row->setData(kConnectionIdRole, item.connectionId);
    row->setData(kTerminalIdRole, item.terminalId);
    row->setData(kEnabledRole, item.enabled);
    row->setFlags(item.enabled ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable)
                               : Qt::ItemFlags(Qt::NoItemFlags));
    if (!item.enabled) {
        row->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
    }
    return row;
}

void CommandPaletteDialog::rebuildVisibleList()
{
    const QString query = m_filterEdit ? m_filterEdit->text() : QString();
    QVector<BuiltItem> items;

    switch (m_mode) {
    case Mode::Actions: {
        for (const ActionItem &action : m_actions) {
            BuiltItem item;
            item.kind = ItemKind::Action;
            item.primary = action.label;
            item.secondary = action.group;
            item.shortcutText = action.shortcutText;
            item.searchFields = {action.label, action.group, action.shortcutText, action.actionId};
            item.enabled = action.enabled;
            item.actionId = action.actionId;
            if (const auto score = FuzzyMatch::bestMatchScore(query, item.searchFields)) {
                item.score = *score;
                items.append(item);
            }
        }
        break;
    }
    case Mode::Connections: {
        for (const ConnectionItem &connection : m_connections) {
            BuiltItem item;
            item.kind = ItemKind::Connection;
            item.primary = connection.name;
            item.secondary = connection.subtitle;
            item.searchFields = connection.searchFields;
            item.connectionId = connection.id;
            item.enabled = true;
            if (const auto score = FuzzyMatch::bestMatchScore(query, item.searchFields)) {
                item.score = *score;
                if (query.trimmed().isEmpty() && connection.recentRank >= 0) {
                    item.score += 1000 - connection.recentRank;
                }
                items.append(item);
            }
        }

        BuiltItem create;
        create.kind = ItemKind::CreateConnection;
        create.enabled = true;
        create.searchFields = {QStringLiteral("create"), query};
        if (query.trimmed().isEmpty()) {
            create.primary = tr("Create connection…");
            create.score = -1;
        } else {
            create.primary = tr("Create “%1”…").arg(query.trimmed());
            create.score = -1;
        }
        // Always show create row.
        items.append(create);
        break;
    }
    case Mode::Terminals: {
        if (m_terminals.isEmpty()) {
            BuiltItem empty;
            empty.kind = ItemKind::EmptyHint;
            empty.primary = tr("No open terminals");
            empty.secondary = tr("Try Quick Connect to open a host");
            empty.enabled = false;
            empty.score = 0;
            items.append(empty);
            break;
        }
        for (const TerminalItem &shell : m_terminals) {
            BuiltItem item;
            item.kind = ItemKind::Terminal;
            item.primary = shell.title;
            item.secondary = shell.subtitle;
            item.searchFields = shell.searchFields;
            item.connectionId = shell.connectionId;
            item.terminalId = shell.terminalId;
            item.enabled = true;
            if (const auto score = FuzzyMatch::bestMatchScore(query, item.searchFields)) {
                item.score = *score;
                if (query.trimmed().isEmpty() && shell.isActive) {
                    item.score += 500;
                }
                items.append(item);
            }
        }
        break;
    }
    }

    std::stable_sort(items.begin(), items.end(), [](const BuiltItem &a, const BuiltItem &b) {
        // Create row always last when scores equal / negative.
        if (a.kind == ItemKind::CreateConnection && b.kind != ItemKind::CreateConnection) {
            return false;
        }
        if (b.kind == ItemKind::CreateConnection && a.kind != ItemKind::CreateConnection) {
            return true;
        }
        return a.score > b.score;
    });

    m_list->clear();
    for (const BuiltItem &item : items) {
        m_list->addItem(makeRow(item));
    }
    if (m_list->count() > 0) {
        // Prefer first enabled row.
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(kEnabledRole).toBool()) {
                m_list->setCurrentRow(i);
                break;
            }
        }
        if (!m_list->currentItem()) {
            m_list->setCurrentRow(0);
        }
    }
}

void CommandPaletteDialog::activateCurrentItem()
{
    QListWidgetItem *row = m_list->currentItem();
    if (!row || !row->data(kEnabledRole).toBool()) {
        return;
    }

    const auto kind = static_cast<ItemKind>(row->data(kKindRole).toInt());
    switch (kind) {
    case ItemKind::Action:
        emit actionChosen(row->data(kActionIdRole).toString());
        accept();
        break;
    case ItemKind::Connection:
        emit connectionChosen(row->data(kConnectionIdRole).toUuid());
        accept();
        break;
    case ItemKind::CreateConnection:
        emit createConnectionChosen(m_filterEdit->text().trimmed());
        accept();
        break;
    case ItemKind::Terminal:
        emit terminalChosen(row->data(kConnectionIdRole).toUuid(),
                            row->data(kTerminalIdRole).toUuid());
        accept();
        break;
    case ItemKind::EmptyHint:
        break;
    }
}
