/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "SftpTypes.h"

#include <QString>

/**
 * Shared symlink path policy and local create/read helpers.
 * Protocol-specific remote create/read stay on FsEngine (SFTP/SCP).
 */
class Symlink
{
public:
    Symlink() = delete;

    /// Pair of symlink path and raw target (named fields avoid swappable QString args).
    struct Spec
    {
        QString linkPath;
        QString target;
    };

    /// Resolve a possibly-relative target against the directory of the link path (POSIX).
    /// Absolute targets (leading '/') are cleaned and returned as-is.
    static QString resolve(const Spec &spec);

    /// Create a local symlink at linkPath pointing to raw target (preserve semantics).
    /// Creates parent directories and replaces any existing file/symlink at linkPath.
    static bool create(const Spec &spec, QString *error = nullptr);

    /// Read the raw target of a local symlink (does not follow).
    static bool read(const QString &path, QString &targetOut, QString *error = nullptr);

    /// True for a real directory or a symlink whose followed target is a directory.
    static bool isDirectoryLike(const RemoteEntry &entry);

    /// Path form that lists through a symlink-to-dir (BusyBox: trailing slash).
    static QString directoryListPath(const QString &path);

    /// Split ls "name -> target" into name and optional raw target.
    struct LsNameParts
    {
        QString name;
        QString target;
    };
    static LsNameParts splitLsName(const QString &rawName);

    /// Normalize readlink-style output (strip trailing CR).
    static QString normalizeTarget(QString target);

    /// Parent directory of a remote-style POSIX path ("/" -> "/", empty -> ".").
    static QString parentDir(const QString &path);
};
