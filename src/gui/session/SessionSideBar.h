/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QWidget>

class QLabel;
class QTabWidget;
class Session;

class SessionSideBar final : public QWidget
{
    Q_OBJECT

public:
    explicit SessionSideBar(QWidget *parent = nullptr);

    void bindSession(Session *session);
    void unbindSession();

    QWidget *fileContainer() const { return m_fileContainer; }
    QWidget *tunnelContainer() const { return m_tunnelContainer; }

private:
    void loadTabState();
    void saveTabState();
    QString currentTabId() const;
    void setCurrentTabId(const QString &tabId);

    QTabWidget *m_tabs = nullptr;
    QWidget *m_fileContainer = nullptr;
    QWidget *m_tunnelContainer = nullptr;
    QLabel *m_placeholder = nullptr;
};
