// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "Symlink.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>

namespace
{
QString trSymlink(const char *text)
{
    return QCoreApplication::translate("Symlink", text);
}
} // namespace

QString Symlink::parentDir(const QString &path)
{
    if (path.isEmpty() || path == QLatin1String("/")) {
        return QStringLiteral("/");
    }
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return QStringLiteral(".");
    }
    if (slash == 0) {
        return QStringLiteral("/");
    }
    return path.left(slash);
}

QString Symlink::resolve(const Spec &spec)
{
    if (spec.target.isEmpty()) {
        return {};
    }
    if (spec.target.startsWith(QLatin1Char('/'))) {
        return QDir::cleanPath(spec.target);
    }
    const QString base = parentDir(spec.linkPath);
    if (base.isEmpty() || base == QLatin1String(".")) {
        return QDir::cleanPath(spec.target);
    }
    if (base == QLatin1String("/")) {
        return QDir::cleanPath(QLatin1Char('/') + spec.target);
    }
    return QDir::cleanPath(base + QLatin1Char('/') + spec.target);
}

bool Symlink::create(const Spec &spec, QString *error)
{
    if (spec.target.isEmpty() || spec.linkPath.isEmpty()) {
        if (error) {
            *error = trSymlink("Invalid symlink arguments");
        }
        return false;
    }

    const QFileInfo localInfo(spec.linkPath);
    const QString parent = localInfo.absolutePath();
    if (!parent.isEmpty() && !QDir().mkpath(parent)) {
        if (error) {
            *error = (errno == ENOSPC) ? trSymlink("Disk full")
                                       : trSymlink("Cannot create local folder: %1").arg(parent);
        }
        return false;
    }

    if (QFileInfo::exists(spec.linkPath) || QFileInfo(spec.linkPath).isSymLink()) {
        QFile::remove(spec.linkPath);
    }

    if (!QFile::link(spec.target, spec.linkPath)) {
        if (error) {
            *error = trSymlink("Cannot create local symlink: %1").arg(spec.linkPath);
        }
        return false;
    }
    return true;
}

bool Symlink::read(const QString &path, QString &targetOut, QString *error)
{
    const QFileInfo info(path);
    if (!info.isSymLink()) {
        if (error) {
            *error = trSymlink("Not a symlink: %1").arg(path);
        }
        return false;
    }
    const QString target = info.readSymLink();
    if (target.isEmpty()) {
        if (error) {
            *error = trSymlink("Cannot read local symlink: %1").arg(path);
        }
        return false;
    }
    targetOut = target;
    return true;
}

bool Symlink::isDirectoryLike(const RemoteEntry &entry)
{
    return entry.isDir || (entry.isSymlink && entry.linkIsDir);
}

QString Symlink::directoryListPath(const QString &path)
{
    // BusyBox `ls symlink` lists the link itself; a trailing slash forces entering the
    // target directory. Harmless for real directories.
    if (path.isEmpty() || path == QLatin1String("/")) {
        return QStringLiteral("/");
    }
    if (path.endsWith(QLatin1Char('/'))) {
        return path;
    }
    return path + QLatin1Char('/');
}

Symlink::LsNameParts Symlink::splitLsName(const QString &rawName)
{
    LsNameParts parts;
    const qsizetype arrow = rawName.indexOf(QStringLiteral(" -> "));
    if (arrow >= 0) {
        parts.name = rawName.left(arrow);
        parts.target = rawName.mid(arrow + 4);
    } else {
        parts.name = rawName;
    }
    return parts;
}

QString Symlink::normalizeTarget(QString target)
{
    while (target.endsWith(QLatin1Char('\r'))) {
        target.chop(1);
    }
    return target;
}
