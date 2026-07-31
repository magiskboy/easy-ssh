/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QList>
#include <QString>
#include <QUuid>

#include <cstdint>

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

enum class SshProxyMode : std::uint8_t
{
    None = 0,
    ProxyJump = 1,
    ProxyCommand = 2,
};

inline bool isSshNoneToken(const QString &value)
{
    return value.trimmed().compare(QLatin1String("none"), Qt::CaseInsensitive) == 0;
}

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

    SshProxyMode proxyMode = SshProxyMode::None;
    QList<JumpHop> jumpHops;
    QString proxyCommand;

    int keepAliveIntervalSec = 0;
    int keepAliveCountMax = 3;
    bool compressionEnabled = false;
    /// ForwardAgent — session option, not an authentication method (see P5).
    bool agentForwarding = false;
    ShellCommandSetConfig shellCommands;

    bool usesJumpHost() const
    {
        return proxyMode == SshProxyMode::ProxyJump && !jumpHops.isEmpty();
    }

    bool usesProxyCommand() const
    {
        return proxyMode == SshProxyMode::ProxyCommand && !proxyCommand.trimmed().isEmpty() &&
               !isSshNoneToken(proxyCommand);
    }

    void normalizeProxyFields()
    {
        if (isSshNoneToken(proxyCommand)) {
            proxyCommand.clear();
            if (proxyMode == SshProxyMode::ProxyCommand) {
                proxyMode = SshProxyMode::None;
            }
        }

        switch (proxyMode) {
        case SshProxyMode::None:
            jumpHops.clear();
            proxyCommand.clear();
            break;
        case SshProxyMode::ProxyJump:
            proxyCommand.clear();
            break;
        case SshProxyMode::ProxyCommand:
            jumpHops.clear();
            break;
        }
    }

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
        } else if (usesProxyCommand()) {
            text += QStringLiteral(" [via ProxyCommand]");
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
