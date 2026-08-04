/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QWidget>

class ExplorerFilterProxy;
class QComboBox;

class ServiceFilterBar final : public QWidget
{
    Q_OBJECT

public:
    explicit ServiceFilterBar(ExplorerFilterProxy *proxy, QWidget *parent = nullptr);

private slots:
    void applyFilters();

private:
    ExplorerFilterProxy *m_proxy = nullptr;
    QComboBox *m_activeCombo = nullptr;
    QComboBox *m_enabledCombo = nullptr;
};
