/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/ExplorerTypes.h"

#include <QObject>
#include <QString>

/// Remote (or local) data feed for a view-only explorer page.
class IExplorerSource : public QObject
{
    Q_OBJECT

public:
    explicit IExplorerSource(QObject *parent = nullptr) : QObject(parent) {}
    ~IExplorerSource() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void refresh() = 0;

    virtual ExplorerUpdateMode updateMode() const = 0;
    virtual ExplorerCapability capability() const = 0;
    virtual QString capabilityMessage() const = 0;

signals:
    void capabilityChanged(ExplorerCapability capability);
    void failed(const QString &error);
    void busyChanged(bool busy);
};
