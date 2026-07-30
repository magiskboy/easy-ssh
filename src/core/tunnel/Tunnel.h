/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QMetaType>
#include <QString>
#include <QUuid>

enum class TunnelType
{
    Local = 0,
    Remote = 1,
    /// Not implement yet (SOCKS / dynamic forward).
    Dynamic = 2,
};

struct TunnelDefinition
{
    QUuid id;
    QUuid connectionId;
    QString name;
    TunnelType type = TunnelType::Local;
    QString localHost = QStringLiteral("127.0.0.1");
    quint16 localPort = 0;
    QString remoteHost = QStringLiteral("127.0.0.1");
    quint16 remotePort = 0;
    bool enabled = true;

    QString localAddress() const { return QStringLiteral("%1:%2").arg(localHost).arg(localPort); }

    QString remoteAddress() const
    {
        return QStringLiteral("%1:%2").arg(remoteHost).arg(remotePort);
    }
};

Q_DECLARE_METATYPE(TunnelDefinition)
