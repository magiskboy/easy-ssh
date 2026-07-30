/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Connection.h"

#include <QList>
#include <QString>
#include <QStringList>

struct SshConfigHost
{
    QString alias;
    QString hostName;
    QString user;
    quint16 port = 22;
    QStringList identityFiles;
    QString proxyJump;
};

/**
 * Discovers concrete Host aliases from OpenSSH config (including Include),
 * then resolves HostName/User/Port/Identity via libssh ssh_options_parse_config.
 *
 * libssh cannot enumerate Host blocks; alias collection remains a thin scan.
 */
class SshConfigParser
{
public:
    static QString defaultConfigPath();
    static QList<SshConfigHost> load(const QString &path = {});
    static QList<Connection> toConnections(const QList<SshConfigHost> &hosts);
    static QUuid stableIdForAlias(const QString &alias);
    static QList<JumpHop> parseProxyJumpHops(const QString &proxyJump);
};
