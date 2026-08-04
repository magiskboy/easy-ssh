/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QtGlobal>

struct ServiceInfo
{
    QString manager; ///< "systemd" (future: openrc, …)
    QString unit;    ///< e.g. sshd.service
    QString description;
    QString loadState;     ///< loaded | not-found | …
    QString activeState;   ///< Normalized: active|inactive|failed|activating|deactivating|unknown
    QString subState;      ///< running, dead, exited, …
    QString unitFileState; ///< enabled, disabled, static, masked, …
    qint64 mainPid = 0;    ///< 0 if unknown / not running

    QString id() const
    {
        if (manager.isEmpty() || unit.isEmpty()) {
            return {};
        }
        return manager + QLatin1Char(':') + unit;
    }
};
