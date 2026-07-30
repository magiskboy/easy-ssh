/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QString>

class QKeySequenceEdit;
class QTreeWidget;

class ShortcutsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutsDialog(QWidget *parent = nullptr);

private slots:
    void accept() override;
    void resetDefaults();

private:
    void loadFromSettings();
    void saveToSettings();

    QTreeWidget *m_tree = nullptr;
    QHash<QString, QKeySequenceEdit *> m_editors;
};
