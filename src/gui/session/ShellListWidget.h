/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QUuid>
#include <QWidget>

class QListWidget;
class Session;

class ShellListWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ShellListWidget(QWidget *parent = nullptr);

    void bindSession(Session *session);
    void unbindSession();

public slots:
    void newShell();

signals:
    /// User asked to show this shell (sidebar click). May repeat for the already-active shell.
    void shellActivationRequested(const QUuid &shellId);

private slots:
    void refresh();
    void onItemClicked();
    void onContextMenu(const QPoint &pos);
    void renameSelected();
    void closeSelected();

private:
    Session *m_session = nullptr;
    QListWidget *m_list = nullptr;
};
