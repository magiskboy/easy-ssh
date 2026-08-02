/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/ErrorNotifier.h"

#include <QUuid>
#include <QWidget>

class ConnectionModel;
class QLabel;
class QListWidget;

class WelcomeWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget *parent = nullptr);

    void setConnectionModel(ConnectionModel *model);
    void refresh();

signals:
    void openConnectionRequested(const QUuid &id);
    void createConnectionRequested();
    void showConnectionsRequested();
    void statusMessage(const QString &message, ErrorNotifier::Level level);

private slots:
    void onRecentActivated();

private:
    void openCurrentRecent();
    void rebuildRecentList();

    ConnectionModel *m_model = nullptr;
    QLabel *m_recentHeading = nullptr;
    QListWidget *m_recentList = nullptr;
    QLabel *m_emptyRecentLabel = nullptr;
};
