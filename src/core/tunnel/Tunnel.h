/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QMetaType>
#include <QString>
#include <QUuid>

#include <cstdint>

enum class TunnelType : std::uint8_t
{
    Local = 0,
    Remote = 1,
    Dynamic = 2,
};

enum class TunnelEndpointKind : std::uint8_t
{
    Tcp = 0,
    UnixSocket = 1,
};

enum class SocksAuthMode : std::uint8_t
{
    None = 0,
    UsernamePassword = 1,
};

struct TunnelDefinition
{
    // Plain data aggregate (DTO); public fields are intentional.
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    QUuid id;
    QUuid connectionId;
    QString name;
    TunnelType type = TunnelType::Local;
    bool enabled = true;

    TunnelEndpointKind localKind = TunnelEndpointKind::Tcp;
    QString localHost = QStringLiteral("127.0.0.1");
    quint16 localPort = 0;
    QString localSocketPath;

    TunnelEndpointKind remoteKind = TunnelEndpointKind::Tcp;
    QString remoteHost = QStringLiteral("127.0.0.1");
    quint16 remotePort = 0;
    QString remoteSocketPath;

    SocksAuthMode socksAuth = SocksAuthMode::None;
    QString socksUsername;
    /// Runtime-only; never persisted by TunnelStore.
    QString socksPassword;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    QString localAddress() const
    {
        if (localKind == TunnelEndpointKind::UnixSocket) {
            return localSocketPath;
        }
        return QStringLiteral("%1:%2").arg(localHost).arg(localPort);
    }

    QString remoteAddress() const
    {
        if (type == TunnelType::Dynamic) {
            return QStringLiteral("SOCKS5");
        }
        if (remoteKind == TunnelEndpointKind::UnixSocket) {
            return remoteSocketPath;
        }
        return QStringLiteral("%1:%2").arg(remoteHost).arg(remotePort);
    }

    /// Returns empty string when valid; otherwise a short reason.
    QString validationError() const
    {
        if (name.trimmed().isEmpty()) {
            return QStringLiteral("Name is required");
        }
        if (connectionId.isNull()) {
            return QStringLiteral("Connection is required");
        }

        auto tcpOk = [](const QString &host, quint16 port) {
            return !host.trimmed().isEmpty() && port > 0;
        };
        auto pathOk = [](const QString &path) {
            const QString trimmed = path.trimmed();
            return !trimmed.isEmpty() && trimmed.startsWith(QLatin1Char('/'));
        };

        switch (type) {
        case TunnelType::Dynamic:
            if (localKind != TunnelEndpointKind::Tcp || !tcpOk(localHost, localPort)) {
                return QStringLiteral("Dynamic tunnel requires a local TCP bind host and port");
            }
            if (socksAuth == SocksAuthMode::UsernamePassword && socksUsername.trimmed().isEmpty()) {
                return QStringLiteral("SOCKS username is required");
            }
            return {};
        case TunnelType::Remote:
            // Remote listen is always TCP on the server.
            if (!tcpOk(remoteHost, remotePort)) {
                return QStringLiteral("Remote listen host and port are required");
            }
            if (localKind == TunnelEndpointKind::UnixSocket) {
                if (!pathOk(localSocketPath)) {
                    return QStringLiteral("Local Unix socket path must be absolute");
                }
            } else if (!tcpOk(localHost, localPort)) {
                return QStringLiteral("Local destination host and port are required");
            }
            return {};
        case TunnelType::Local:
            if (localKind == TunnelEndpointKind::UnixSocket) {
                if (!pathOk(localSocketPath)) {
                    return QStringLiteral("Local Unix socket path must be absolute");
                }
            } else if (!tcpOk(localHost, localPort)) {
                return QStringLiteral("Local bind host and port are required");
            }
            if (remoteKind == TunnelEndpointKind::UnixSocket) {
                if (!pathOk(remoteSocketPath)) {
                    return QStringLiteral("Remote Unix socket path must be absolute");
                }
            } else if (!tcpOk(remoteHost, remotePort)) {
                return QStringLiteral("Remote destination host and port are required");
            }
            return {};
        }
        return QStringLiteral("Invalid tunnel type");
    }

    bool isValid() const { return validationError().isEmpty(); }
};

Q_DECLARE_METATYPE(TunnelDefinition)
