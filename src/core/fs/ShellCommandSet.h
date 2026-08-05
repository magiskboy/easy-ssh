/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include "SftpTypes.h"

/**
 * WinSCP-style command templates for SCP+shell remote FS.
 * Empty config fields resolve to built-in Unix defaults.
 */
class ShellCommandSet
{
public:
    ShellCommandSet() = default;
    explicit ShellCommandSet(const ShellCommandSetConfig &config);

    void setConfig(const ShellCommandSetConfig &config);
    const ShellCommandSetConfig &config() const { return m_config; }

    static QString shellQuote(const QString &arg);

    QString listingBase() const;
    QString formatListDirectory(const QString &path, const QString &extraOptions) const;
    QString formatListFile(const QString &path, const QString &extraOptions) const;
    QString formatMkdir(const QString &path) const;
    QString formatRemove(const QString &path) const;
    QString formatRename(const QString &from, const QString &to) const;
    QString formatPwd() const;
    QString formatRealpath(const QString &path) const;
    QString formatSymlink(const QString &target, const QString &linkPath) const;
    QString formatReadlink(const QString &path) const;
    /// Exit 0 if path is a directory after following symlinks.
    QString formatTestDirectory(const QString &path) const;

    /// One-shot startup hygiene (unalias / unset), empty if disabled.
    QString formatStartupCommands() const;

    static bool parseLsListing(const QString &output,
                               QVector<RemoteEntry> *outEntries,
                               const QString &parentPath,
                               QString *error = nullptr);

    static bool parseLsSingle(const QString &output,
                              RemoteEntry *outEntry,
                              const QString &path,
                              QString *error = nullptr);

private:
    QString resolve(const QString &configured, const QString &fallback) const;
    QString applyTemplate(const QString &tmpl, const QStringList &args) const;

    ShellCommandSetConfig m_config;
};
