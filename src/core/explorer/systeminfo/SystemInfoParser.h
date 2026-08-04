/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/ExplorerTypes.h"
#include "core/explorer/systeminfo/SystemInfo.h"

#include <QByteArray>
#include <QString>

namespace SystemInfoParser
{

/// Single remote script: dump /proc + df as one JSON object.
QString fetchCommand();

bool parseSnapshot(const QByteArray &stdoutBytes, SystemInfo *out, QString *error = nullptr);

ExplorerCapability classifyFailure(int exitStatus,
                                   const QByteArray &stderrBytes,
                                   const QString &errorMessage,
                                   QString *messageOut = nullptr);

QString formatBytes(quint64 bytes);
QString formatBytesFromKiB(quint64 kib);
QString formatUptime(qint64 seconds);
QString formatPercent(double percent);
QString formatRateBps(double bps);
QString formatFreqKHz(qint64 kHz);
QString formatIops(double iops);
QString formatCelsius(double celsius);
QString formatLinkSpeed(qint64 speedMbps);

/// Human-readable multi-line snapshot for clipboard / tickets.
QString formatSnapshotText(const SystemInfo &info);
/// Compact JSON object built from the typed snapshot.
QString formatSnapshotJson(const SystemInfo &info);

} // namespace SystemInfoParser
