/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QtGlobal>

struct ContainerInfo
{
    QString runtime;     ///< "podman" | "docker" | "containerd"
    QString containerId; ///< Full id from the engine
    QString name;
    QString image;
    QString state;            ///< Normalized: running|exited|created|paused|unknown
    qint64 pid = 0;           ///< 0 if unknown / not running
    QString runtimeNamespace; ///< containerd only (e.g. k8s.io, default)
    double cpuPercent = -1.0; ///< -1 if unknown
    double memPercent = -1.0; ///< -1 if unknown
    QString memUsage;         ///< e.g. "21.4MiB / 65MiB"

    QString id() const
    {
        if (runtime.isEmpty() || containerId.isEmpty()) {
            return {};
        }
        return runtime + QLatin1Char(':') + containerId;
    }
};
