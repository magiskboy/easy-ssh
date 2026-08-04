/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/ExplorerTypes.h"
#include "core/explorer/process/ProcessInfo.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace ProcessParser
{

/// GNU/procps list with fields needed for table + detail pane.
QString listCommand();

bool parsePsList(const QByteArray &stdoutBytes,
                 QVector<ProcessInfo> *out,
                 QString *error = nullptr);

ExplorerCapability classifyFailure(int exitStatus,
                                   const QByteArray &stderrBytes,
                                   const QString &errorMessage,
                                   QString *messageOut = nullptr);

/// Map first character of ps STAT to a localized full status label.
QString formatStateDisplay(const QString &stateCode);

/// e.g. "Normal (0)", "High (-5)", "Low (10)" from nice value.
QString formatPriorityDisplay(int nice);

/// Human start time from etimes (seconds ago).
QString formatStartedDisplay(qint64 elapsedSeconds);

/// Format KiB from ps rss/vsz as MiB / GiB / TiB.
QString formatMemoryFromKiB(qint64 kib);

QString formatUserDisplay(const ProcessInfo &info);

QString displayName(const ProcessInfo &info);

} // namespace ProcessParser
