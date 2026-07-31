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
    QString proxyCommand;
    bool forwardAgent = false;
};

struct ProxyJumpParseRequest
{
    QString proxyJump;
    QString configPath;
};

/**
 * OpenSSH config → Connection materialization.
 *
 * libssh cannot enumerate Host blocks, so alias discovery remains a thin file
 * scan (including Include). All resolvable options (HostName, User, Port,
 * IdentityFile, ProxyCommand) come from ssh_options_parse_config.
 *
 * ProxyJump and ForwardAgent are not readable via ssh_options_get (ProxyJump is
 * write-only; ForwardAgent is unsupported by libssh's config parser). Those two
 * keywords use an Include-correct keyword scan only, then ProxyJump hop hosts
 * are expanded again through libssh so aliases like `lab-bastion` become
 * concrete HostName/User/Port/Identity values for App connections after import.
 */
class SshConfigParser
{
public:
    static QString defaultConfigPath();
    static QList<SshConfigHost> load(const QString &path = {});
    static QList<Connection> toConnections(const QList<SshConfigHost> &hosts,
                                           const QString &configPath = {});
    static QUuid stableIdForAlias(const QString &alias);
    static QList<JumpHop> parseProxyJumpHops(const ProxyJumpParseRequest &request);
};
