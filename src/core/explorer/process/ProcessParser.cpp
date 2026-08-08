// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ProcessParser.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>
#include <QtMath>

namespace ProcessParser
{
namespace
{
QString trParse(const char *text)
{
    return QCoreApplication::translate("ProcessParser", text);
}

bool looksLikeCommandMissing(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("command not found")) ||
           hay.contains(QLatin1String("not found")) ||
           hay.contains(QLatin1String("no such file")) ||
           hay.contains(QLatin1String("executable file not found"));
}

bool looksLikePermissionDenied(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("permission denied")) ||
           hay.contains(QLatin1String("operation not permitted")) ||
           hay.contains(QLatin1String("access denied"));
}
} // namespace

QString listCommand()
{
    // Fixed fields first; args last (may contain spaces).
    // pid ppid uid user pcpu pmem stat nice pri etimes cputime rss vsz comm args
    return QStringLiteral("ps -eo "
                          "pid=,ppid=,uid=,user=,pcpu=,pmem=,stat=,nice=,pri=,etimes=,cputime=,rss="
                          ",vsz=,comm=,args=");
}

bool parsePsList(const QByteArray &stdoutBytes, QVector<ProcessInfo> *out, QString *error)
{
    if (!out) {
        if (error) {
            *error = trParse("Output buffer is null");
        }
        return false;
    }
    out->clear();

    const QString text = QString::fromUtf8(stdoutBytes);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    out->reserve(lines.size());

    constexpr int kFixedFieldCount = 14; // through comm=

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts =
            line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < kFixedFieldCount) {
            continue;
        }

        bool okPid = false;
        ProcessInfo info;
        info.pid = parts.at(0).toLongLong(&okPid);
        if (!okPid || info.pid <= 0) {
            continue;
        }

        info.ppid = parts.at(1).toLongLong();
        info.uid = parts.at(2).toLongLong();
        info.user = parts.at(3);
        info.cpuPercent = parts.at(4).toDouble();
        info.memPercent = parts.at(5).toDouble();
        info.stateCode = parts.at(6);
        info.nice = parts.at(7).toInt();
        info.priority = parts.at(8).toInt();
        info.elapsedSeconds = parts.at(9).toLongLong();
        info.cpuTime = parts.at(10);
        info.rssKiB = parts.at(11).toLongLong();
        info.vszKiB = parts.at(12).toLongLong();
        info.comm = parts.at(13);
        if (parts.size() > kFixedFieldCount) {
            info.command = QStringList(parts.mid(kFixedFieldCount)).join(QLatin1Char(' '));
        } else {
            info.command = info.comm;
        }

        out->append(info);
    }

    return true;
}

ExplorerCapability classifyFailure(int exitStatus,
                                   const QByteArray &stderrBytes,
                                   const QString &errorMessage,
                                   QString *messageOut)
{
    const QString stderrText = QString::fromUtf8(stderrBytes).trimmed();
    const QString detail =
        !errorMessage.isEmpty()
            ? errorMessage
            : (!stderrText.isEmpty() ? stderrText
                                     : trParse("Remote command failed (exit %1)").arg(exitStatus));

    if (looksLikeCommandMissing(stderrText, errorMessage)) {
        if (messageOut) {
            *messageOut = trParse("ps is not available on this host.");
        }
        return ExplorerCapability::Unavailable;
    }
    if (looksLikePermissionDenied(stderrText, errorMessage) || exitStatus == 126) {
        if (messageOut) {
            *messageOut = trParse("Permission denied while listing processes.");
        }
        return ExplorerCapability::PermissionDenied;
    }
    if (messageOut) {
        *messageOut = detail;
    }
    return ExplorerCapability::Error;
}

QString formatStateDisplay(const QString &stateCode)
{
    if (stateCode.isEmpty()) {
        return trParse("Unknown");
    }
    switch (stateCode.at(0).toLatin1()) {
    case 'R':
        return trParse("Running");
    case 'S':
        return trParse("Sleeping");
    case 'D':
        return trParse("Waiting");
    case 'T':
        return trParse("Stopped");
    case 't':
        return trParse("Tracing stop");
    case 'Z':
        return trParse("Zombie");
    case 'X':
    case 'x':
        return trParse("Dead");
    case 'I':
        return trParse("Idle");
    case 'K':
        return trParse("Wake kill");
    case 'W':
        return trParse("Paging");
    case 'P':
        return trParse("Parked");
    default:
        return stateCode;
    }
}

QString formatPriorityDisplay(int nice)
{
    if (nice < -10) {
        return trParse("Very high (%1)").arg(nice);
    }
    if (nice < 0) {
        return trParse("High (%1)").arg(nice);
    }
    if (nice == 0) {
        return trParse("Normal (%1)").arg(nice);
    }
    if (nice <= 10) {
        return trParse("Low (%1)").arg(nice);
    }
    return trParse("Very low (%1)").arg(nice);
}

QString formatStartedDisplay(qint64 elapsedSeconds, const QDateTime &now)
{
    if (elapsedSeconds < 0) {
        return QStringLiteral("—");
    }
    const QDateTime started = now.addSecs(-elapsedSeconds);
    const QDate today = now.date();
    const QString timeText = QLocale::system().toString(started.time(), QLocale::ShortFormat);
    if (started.date() == today) {
        return trParse("Today %1").arg(timeText);
    }
    if (started.date() == today.addDays(-1)) {
        return trParse("Yesterday %1").arg(timeText);
    }
    return QLocale::system().toString(started, QLocale::ShortFormat);
}

QString formatMemoryFromKiB(qint64 kib)
{
    if (kib < 0) {
        return QStringLiteral("—");
    }
    const double bytes = static_cast<double>(kib) * 1024.0;
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * 1024.0;
    constexpr double kGiB = kMiB * 1024.0;
    constexpr double kTiB = kGiB * 1024.0;

    if (bytes >= kTiB) {
        return trParse("%1 TiB").arg(bytes / kTiB, 0, 'f', 1);
    }
    if (bytes >= kGiB) {
        return trParse("%1 GiB").arg(bytes / kGiB, 0, 'f', 1);
    }
    if (bytes >= kMiB) {
        return trParse("%1 MiB").arg(bytes / kMiB, 0, 'f', 1);
    }
    if (bytes >= kKiB) {
        return trParse("%1 KiB").arg(bytes / kKiB, 0, 'f', 0);
    }
    return trParse("%1 B").arg(qRound(bytes));
}

QString formatUserDisplay(const ProcessInfo &info)
{
    if (info.uid >= 0 && !info.user.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(info.user).arg(info.uid);
    }
    if (!info.user.isEmpty()) {
        return info.user;
    }
    if (info.uid >= 0) {
        return QString::number(info.uid);
    }
    return QStringLiteral("—");
}

QString displayName(const ProcessInfo &info)
{
    if (!info.comm.isEmpty()) {
        return info.comm;
    }
    if (!info.command.isEmpty()) {
        const QStringList parts = info.command.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            const QString base = parts.first();
            const int slash = base.lastIndexOf(QLatin1Char('/'));
            return slash >= 0 ? base.mid(slash + 1) : base;
        }
    }
    return QString::number(info.pid);
}

} // namespace ProcessParser
