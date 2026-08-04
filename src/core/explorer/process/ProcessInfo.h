/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QtGlobal>

struct ProcessInfo
{
    qint64 pid = 0;
    qint64 ppid = 0;
    qint64 uid = -1;
    QString user;
    double cpuPercent = 0.0;
    double memPercent = 0.0;
    QString stateCode; ///< Raw ps STAT (e.g. "Ssl")
    int nice = 0;
    int priority = 0;
    qint64 elapsedSeconds = -1; ///< etimes; -1 if unknown
    QString cpuTime;            ///< cputime / time [[DD-]HH:]MM:SS
    qint64 rssKiB = 0;
    qint64 vszKiB = 0;
    QString comm;    ///< Short process name
    QString command; ///< Full args

    QString id() const { return QString::number(pid); }
};
