/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/container/ContainerInfo.h"

#include <QJsonObject>
#include <QString>

/// Pure helper contract for a mid-level container engine backend.
class IContainerBackend
{
public:
    virtual ~IContainerBackend() = default;

    virtual QString runtimeId() const = 0;
    /// Shell fragment that prints normalized NDJSON lines when the backend is selected.
    virtual QString remoteListSnippet() const = 0;
    /// Optional CPU/memory stats NDJSON (`kind`=`stats`). Empty when unsupported.
    virtual QString remoteStatsSnippet() const { return {}; }
    virtual bool parseLine(const QJsonObject &object, ContainerInfo *out) const = 0;
};
