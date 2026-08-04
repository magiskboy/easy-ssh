/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/ExplorerTypes.h"
#include "core/explorer/service/ServiceInfo.h"
#include "core/explorer/service/ServiceInspectInfo.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace ServiceParser
{

QString listCommand();

bool parseList(const QByteArray &stdoutBytes, QVector<ServiceInfo> *out, QString *error = nullptr);

QString inspectCommand(const ServiceInfo &info);

/// Interactive follow command for a unit journal (no trailing newline). Empty if unsupported.
QString followLogsCommand(const ServiceInfo &info, int lines = 100);

bool parseInspect(const QByteArray &stdoutBytes,
                  const ServiceInfo &seed,
                  ServiceInspectInfo *out,
                  QString *error = nullptr);

ExplorerCapability classifyFailure(int exitStatus,
                                   const QByteArray &stderrBytes,
                                   const QString &errorMessage,
                                   QString *messageOut = nullptr);

QString normalizeActiveState(const QString &rawState);
QString formatActiveStateDisplay(const QString &normalizedState);
QString formatOrDash(const QString &value);
QString formatPidDisplay(qint64 pid);

} // namespace ServiceParser
