/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/ErrorNotifier.h"

#include <QStringView>
#include <QUuid>
#include <QWidget>

class ConnectionModel;
class QLabel;
class QListWidget;
class QGridLayout;

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
    void refreshShortcutHints();

private:
    void openCurrentRecent();
    void rebuildRecentList();
    void addShortcutHintRow(int row, QStringView actionId, const QString &fallbackLabel);

    ConnectionModel *m_model = nullptr;
    QLabel *m_recentHeading = nullptr;
    QListWidget *m_recentList = nullptr;
    QLabel *m_emptyRecentLabel = nullptr;
    QLabel *m_shortcutsHeading = nullptr;
    QGridLayout *m_shortcutsGrid = nullptr;
};
