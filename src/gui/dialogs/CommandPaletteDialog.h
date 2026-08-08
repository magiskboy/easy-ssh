/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QUuid>
#include <QVector>

class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

class CommandPaletteDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        Actions,
        Connections,
        Terminals,
    };

    struct ActionItem
    {
        QString actionId;
        QString label;
        QString group;
        QString shortcutText;
        bool enabled = true;
    };

    struct ConnectionItem
    {
        QUuid id;
        QString name;
        QString subtitle;
        QStringList searchFields;
        int recentRank = -1; // lower = more recent; -1 = not recent
    };

    struct TerminalItem
    {
        QUuid connectionId;
        QUuid terminalId;
        QString title;
        QString subtitle;
        QStringList searchFields;
        bool isActive = false;
    };

    explicit CommandPaletteDialog(QWidget *parent = nullptr);

    void setActionItems(const QList<ActionItem> &items);
    void setConnectionItems(const QList<ConnectionItem> &items);
    void setTerminalItems(const QList<TerminalItem> &items);

    void openMode(Mode mode);

signals:
    void actionChosen(const QString &actionId);
    void connectionChosen(const QUuid &connectionId);
    void createConnectionChosen(const QString &query);
    void terminalChosen(const QUuid &connectionId, const QUuid &terminalId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class ItemKind
    {
        Action,
        Connection,
        Terminal,
        CreateConnection,
        EmptyHint,
    };

    struct BuiltItem
    {
        ItemKind kind = ItemKind::EmptyHint;
        QString primary;
        QString secondary;
        QString shortcutText;
        QStringList searchFields;
        int score = 0;
        bool enabled = true;
        QString actionId;
        QUuid connectionId;
        QUuid terminalId;
    };

    void rebuildVisibleList();
    void activateCurrentItem();
    void centerOnParent();
    QListWidgetItem *makeRow(const BuiltItem &item);

    Mode m_mode = Mode::Actions;
    QLineEdit *m_filterEdit = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_hintLabel = nullptr;

    QList<ActionItem> m_actions;
    QList<ConnectionItem> m_connections;
    QList<TerminalItem> m_terminals;
};
