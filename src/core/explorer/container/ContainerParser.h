/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/ExplorerTypes.h"
#include "core/explorer/container/ContainerInfo.h"
#include "core/explorer/container/ContainerInspectInfo.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace ContainerParser
{

QString listCommand();

bool parseList(const QByteArray &stdoutBytes,
               QVector<ContainerInfo> *out,
               QString *error = nullptr);

/// Remote inspect for one container (podman/docker/ctr). Empty if runtime unsupported.
QString inspectCommand(const ContainerInfo &info);

bool parseInspect(const QByteArray &stdoutBytes,
                  const ContainerInfo &seed,
                  ContainerInspectInfo *out,
                  QString *error = nullptr);

ExplorerCapability classifyFailure(int exitStatus,
                                   const QByteArray &stderrBytes,
                                   const QString &errorMessage,
                                   QString *messageOut = nullptr);

QString normalizeState(const QString &rawState);
QString formatStateDisplay(const QString &normalizedState);
QString shortId(const QString &containerId);
QString displayName(const ContainerInfo &info);
QString formatOrDash(const QString &value);
QString joinElidable(const QStringList &parts, const QString &sep = QStringLiteral(", "));
QString formatCpuDisplay(double cpuPercent);
QString formatMemPercentDisplay(double memPercent);
double parsePercentValue(const QString &raw);

} // namespace ContainerParser
