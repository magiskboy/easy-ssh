/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/service/ServiceInfo.h"

#include <QJsonObject>
#include <QString>

/// Pure helper contract for a service-manager backend (systemd, later OpenRC, …).
class IServiceBackend
{
public:
    virtual ~IServiceBackend() = default;

    virtual QString managerId() const = 0;
    /// Shell fragment that prints normalized inventory when the backend is selected.
    /// Prefer JSON when available on the host; otherwise NDJSON lines.
    virtual QString remoteListSnippet() const = 0;
    virtual bool parseLine(const QJsonObject &object, ServiceInfo *out) const = 0;
};
