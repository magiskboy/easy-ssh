/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QList>
#include <QString>
#include <QUuid>

enum class AuthType
{
    Password = 0,
    PrivateKey = 1,
};

enum class ConnectionSource
{
    App = 0,
    SshConfig = 1,
};

struct JumpHop
{
    QString host;
    quint16 port = 22;
    QString username;
    AuthType authType = AuthType::Password;
    QString privateKeyPath;
    bool useTargetCredentials = true;
};

/// WinSCP-style shell + command templates for SCP remote FS fallback.
/// Empty template strings resolve to built-in Unix defaults at runtime.
struct ShellCommandSetConfig
{
    QString shell;
    QString listingCommand;
    QString listFileCommand;
    QString mkdirCommand;
    QString removeCommand;
    QString renameCommand;
    QString pwdCommand;
    QString realpathCommand;
    bool clearAliases = true;
    bool clearNationalVars = true;
    bool tryFullTime = true;
    bool ignoreLsWarnings = false;
    bool allowScpFallback = true;
};

struct Connection
{
    QUuid id;
    QString name;
    QString host;
    quint16 port = 22;
    QString username;
    AuthType authType = AuthType::Password;
    QString privateKeyPath;
    QString startupDirectory;
    ConnectionSource source = ConnectionSource::App;
    QString configAlias;

    QList<JumpHop> jumpHops;

    int keepAliveIntervalSec = 0;
    int keepAliveCountMax = 3;
    bool compressionEnabled = false;
    ShellCommandSetConfig shellCommands;

    bool usesJumpHost() const { return !jumpHops.isEmpty(); }

    QString proxyJumpString() const
    {
        QStringList hops;
        for (const JumpHop &hop : jumpHops) {
            if (hop.host.isEmpty() || hop.username.isEmpty()) {
                continue;
            }
            QString entry = hop.username + QLatin1Char('@') + hop.host;
            if (hop.port != 22) {
                entry += QLatin1Char(':') + QString::number(hop.port);
            }
            hops.append(entry);
        }
        return hops.join(QLatin1Char(','));
    }

    QString displayText() const
    {
        QString text = QStringLiteral("%1 — %2@%3:%4").arg(name, username, host).arg(port);
        if (usesJumpHost()) {
            text += QStringLiteral(" [via gateway]");
        }
        if (source == ConnectionSource::SshConfig) {
            text += QStringLiteral(" [ssh config]");
        }
        return text;
    }
};

struct SessionCredentials
{
    QString targetSecret;
    QString gatewaySecret;
};
