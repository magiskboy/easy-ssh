// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionQuery.h"

#include <QUuid>

namespace
{

bool parseHostPort(const QString &text, QString *hostOut, quint16 *portOut)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    // IPv6 in brackets: [addr]:port or [addr]
    if (trimmed.startsWith(QLatin1Char('['))) {
        const int close = trimmed.indexOf(QLatin1Char(']'));
        if (close <= 1) {
            return false;
        }
        *hostOut = trimmed.mid(1, close - 1);
        if (close + 1 < trimmed.size()) {
            if (trimmed.at(close + 1) != QLatin1Char(':')) {
                return false;
            }
            bool ok = false;
            const int port = trimmed.mid(close + 2).toInt(&ok);
            if (!ok || port < 1 || port > 65535) {
                return false;
            }
            *portOut = static_cast<quint16>(port);
        }
        return !hostOut->isEmpty();
    }

    const int colon = trimmed.lastIndexOf(QLatin1Char(':'));
    if (colon > 0) {
        const QString hostPart = trimmed.left(colon);
        const QString portPart = trimmed.mid(colon + 1);
        // Avoid treating IPv6 without brackets as host:port (multiple colons).
        if (!hostPart.contains(QLatin1Char(':')) && !portPart.isEmpty()) {
            bool ok = false;
            const int port = portPart.toInt(&ok);
            if (ok && port >= 1 && port <= 65535) {
                *hostOut = hostPart;
                *portOut = static_cast<quint16>(port);
                return true;
            }
        }
    }

    *hostOut = trimmed;
    return true;
}

} // namespace

namespace ConnectionQuery
{

Connection draftFromQuery(const QString &query)
{
    Connection draft;
    draft.id = QUuid::createUuid();
    draft.port = 22;

    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return draft;
    }

    QString user;
    QString hostPort = trimmed;
    const int at = trimmed.indexOf(QLatin1Char('@'));
    if (at > 0) {
        user = trimmed.left(at).trimmed();
        hostPort = trimmed.mid(at + 1).trimmed();
    }

    QString host;
    quint16 port = 22;
    if (!parseHostPort(hostPort, &host, &port) || host.isEmpty()) {
        // Fall back: treat whole query as name/host.
        draft.name = trimmed;
        draft.host = trimmed;
        return draft;
    }

    draft.host = host;
    draft.port = port;
    if (!user.isEmpty()) {
        draft.username = user;
    }

    if (!user.isEmpty()) {
        draft.name = user + QLatin1Char('@') + host;
        if (port != 22) {
            draft.name += QLatin1Char(':') + QString::number(port);
        }
    } else {
        draft.name = host;
        if (port != 22) {
            draft.name += QLatin1Char(':') + QString::number(port);
        }
    }

    return draft;
}

} // namespace ConnectionQuery
