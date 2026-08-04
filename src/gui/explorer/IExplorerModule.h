/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/widgets/ExplorerColumn.h"
#include "gui/widgets/IExplorerDetailFactory.h"

#include <QList>
#include <QString>
#include <memory>

class ExplorerTableModel;
class IExplorerSource;
class QObject;
class QWidget;
class Session;

/// Domain kit that every view-only explorer must provide.
class IExplorerModule
{
public:
    virtual ~IExplorerModule() = default;

    virtual QString id() const = 0;
    virtual QString title() const = 0;

    virtual QList<ExplorerColumn> columns() const = 0;
    virtual QList<int> searchColumns() const = 0;

    virtual ExplorerTableModel *createModel(QObject *parent) = 0;
    virtual IExplorerSource *createSource(Session *session, QObject *parent) = 0;
    /// Optional; return nullptr when search-only is enough.
    virtual QWidget *createFilterBar(class ExplorerFilterProxy *proxy, QWidget *parent) = 0;
    virtual std::unique_ptr<IExplorerDetailFactory> createDetailFactory() = 0;
};
