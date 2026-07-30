// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TunnelStore.h"

#include <QSettings>

namespace
{

QString tunnelTypeToString(TunnelType type)
{
    switch (type) {
    case TunnelType::Remote:
        return QStringLiteral("remote");
    case TunnelType::Dynamic:
        return QStringLiteral("dynamic");
    case TunnelType::Local:
        return QStringLiteral("local");
    }
    return QStringLiteral("local");
}

TunnelType tunnelTypeFromString(const QString &value)
{
    if (value == QLatin1String("remote")) {
        return TunnelType::Remote;
    }
    if (value == QLatin1String("dynamic")) {
        return TunnelType::Dynamic;
    }
    return TunnelType::Local;
}

} // namespace

QList<TunnelDefinition> TunnelStore::load()
{
    QSettings settings;
    QList<TunnelDefinition> tunnels;

    const int size = settings.beginReadArray(QStringLiteral("tunnels"));
    tunnels.reserve(size);

    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        TunnelDefinition tunnel;
        tunnel.id = QUuid::fromString(settings.value(QStringLiteral("id")).toString());
        tunnel.connectionId =
            QUuid::fromString(settings.value(QStringLiteral("connectionId")).toString());
        tunnel.name = settings.value(QStringLiteral("name")).toString();
        tunnel.type = tunnelTypeFromString(settings.value(QStringLiteral("type")).toString());
        tunnel.localHost =
            settings.value(QStringLiteral("localHost"), QStringLiteral("127.0.0.1")).toString();
        tunnel.localPort =
            static_cast<quint16>(settings.value(QStringLiteral("localPort"), 0).toUInt());
        tunnel.remoteHost =
            settings.value(QStringLiteral("remoteHost"), QStringLiteral("127.0.0.1")).toString();
        tunnel.remotePort =
            static_cast<quint16>(settings.value(QStringLiteral("remotePort"), 0).toUInt());
        tunnel.enabled = settings.value(QStringLiteral("enabled"), true).toBool();

        if (tunnel.id.isNull() || tunnel.connectionId.isNull() || tunnel.name.isEmpty()) {
            continue;
        }
        if (tunnel.localPort == 0 || tunnel.remotePort == 0) {
            continue;
        }
        tunnels.append(tunnel);
    }

    settings.endArray();
    return tunnels;
}

void TunnelStore::save(const QList<TunnelDefinition> &tunnels)
{
    QSettings settings;
    settings.remove(QStringLiteral("tunnels"));
    settings.beginWriteArray(QStringLiteral("tunnels"), tunnels.size());

    for (int i = 0; i < tunnels.size(); ++i) {
        const TunnelDefinition &tunnel = tunnels.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), tunnel.id.toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("connectionId"),
                          tunnel.connectionId.toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("name"), tunnel.name);
        settings.setValue(QStringLiteral("type"), tunnelTypeToString(tunnel.type));
        settings.setValue(QStringLiteral("localHost"), tunnel.localHost);
        settings.setValue(QStringLiteral("localPort"), tunnel.localPort);
        settings.setValue(QStringLiteral("remoteHost"), tunnel.remoteHost);
        settings.setValue(QStringLiteral("remotePort"), tunnel.remotePort);
        settings.setValue(QStringLiteral("enabled"), tunnel.enabled);
    }

    settings.endArray();
    settings.sync();
}

QList<TunnelDefinition> TunnelStore::loadForConnection(const QUuid &connectionId)
{
    QList<TunnelDefinition> result;
    if (connectionId.isNull()) {
        return result;
    }

    const QList<TunnelDefinition> all = load();
    for (const TunnelDefinition &tunnel : all) {
        if (tunnel.connectionId == connectionId) {
            result.append(tunnel);
        }
    }
    return result;
}

void TunnelStore::removeByConnectionId(const QUuid &connectionId)
{
    if (connectionId.isNull()) {
        return;
    }

    QList<TunnelDefinition> remaining;
    const QList<TunnelDefinition> all = load();
    for (const TunnelDefinition &tunnel : all) {
        if (tunnel.connectionId != connectionId) {
            remaining.append(tunnel);
        }
    }
    save(remaining);
}
