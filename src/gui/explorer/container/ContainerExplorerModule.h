/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/explorer/IExplorerModule.h"

class Session;

class ContainerExplorerModule final : public IExplorerModule
{
public:
    QString id() const override;
    QString title() const override;

    QList<ExplorerColumn> columns() const override;
    QList<int> searchColumns() const override;

    ExplorerTableModel *createModel(QObject *parent) override;
    IExplorerSource *createSource(Session *session, QObject *parent) override;
    QWidget *createFilterBar(ExplorerFilterProxy *proxy, QWidget *parent) override;
    std::unique_ptr<IExplorerDetailFactory> createDetailFactory() override;
    void
    connectSource(IExplorerSource *source, ExplorerTableModel *model, QObject *context) override;

private:
    Session *m_session = nullptr;
};
