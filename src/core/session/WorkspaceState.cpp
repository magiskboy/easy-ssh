// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "WorkspaceState.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QString uuidToString(const QUuid &id)
{
    return id.toString(QUuid::WithoutBraces);
}

QUuid uuidFromString(const QString &text)
{
    const QUuid id = QUuid(text);
    return id.isNull() ? QUuid{} : id;
}
} // namespace

QByteArray WorkspaceState::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("v"), version);
    if (!activeConnectionId.isNull()) {
        root.insert(QStringLiteral("activeConnectionId"), uuidToString(activeConnectionId));
    }

    QJsonArray sessionsJson;
    for (const WorkspaceSessionEntry &session : sessions) {
        if (session.connectionId.isNull()) {
            continue;
        }
        QJsonObject obj;
        obj.insert(QStringLiteral("connectionId"), uuidToString(session.connectionId));
        if (!session.activeShellId.isNull()) {
            obj.insert(QStringLiteral("activeShellId"), uuidToString(session.activeShellId));
        }
        if (!session.activeToolId.isEmpty()) {
            obj.insert(QStringLiteral("activeToolId"), session.activeToolId);
        }
        QJsonArray shellsJson;
        for (const WorkspaceShellEntry &shell : session.shells) {
            if (shell.id.isNull()) {
                continue;
            }
            QJsonObject shellObj;
            shellObj.insert(QStringLiteral("id"), uuidToString(shell.id));
            if (!shell.title.isEmpty()) {
                shellObj.insert(QStringLiteral("title"), shell.title);
            }
            shellsJson.append(shellObj);
        }
        obj.insert(QStringLiteral("shells"), shellsJson);
        if (!session.tools.isEmpty()) {
            QJsonArray toolsJson;
            for (const QString &toolId : session.tools) {
                if (!toolId.isEmpty()) {
                    toolsJson.append(toolId);
                }
            }
            if (!toolsJson.isEmpty()) {
                obj.insert(QStringLiteral("tools"), toolsJson);
            }
        }
        if (!session.dockState.isEmpty()) {
            obj.insert(QStringLiteral("dockState"),
                       QString::fromLatin1(session.dockState.toBase64()));
        }
        sessionsJson.append(obj);
    }
    root.insert(QStringLiteral("sessions"), sessionsJson);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

WorkspaceState WorkspaceState::fromJson(const QByteArray &json, bool *ok)
{
    WorkspaceState state;
    if (ok) {
        *ok = false;
    }
    if (json.isEmpty()) {
        return state;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return state;
    }

    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("v")).toInt(0);
    if (version < 1 || version > kCurrentVersion) {
        return state;
    }

    state.version = version;
    state.activeConnectionId =
        uuidFromString(root.value(QStringLiteral("activeConnectionId")).toString());

    const QJsonArray sessionsJson = root.value(QStringLiteral("sessions")).toArray();
    for (const QJsonValue &value : sessionsJson) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        WorkspaceSessionEntry session;
        session.connectionId = uuidFromString(obj.value(QStringLiteral("connectionId")).toString());
        if (session.connectionId.isNull()) {
            continue;
        }
        session.activeShellId =
            uuidFromString(obj.value(QStringLiteral("activeShellId")).toString());
        session.activeToolId = obj.value(QStringLiteral("activeToolId")).toString();
        const QJsonArray shellsJson = obj.value(QStringLiteral("shells")).toArray();
        for (const QJsonValue &shellValue : shellsJson) {
            if (!shellValue.isObject()) {
                continue;
            }
            const QJsonObject shellObj = shellValue.toObject();
            WorkspaceShellEntry shell;
            shell.id = uuidFromString(shellObj.value(QStringLiteral("id")).toString());
            if (shell.id.isNull()) {
                continue;
            }
            shell.title = shellObj.value(QStringLiteral("title")).toString();
            session.shells.append(shell);
        }
        const QJsonArray toolsJson = obj.value(QStringLiteral("tools")).toArray();
        for (const QJsonValue &toolValue : toolsJson) {
            const QString toolId = toolValue.toString();
            if (!toolId.isEmpty() && !session.tools.contains(toolId)) {
                session.tools.append(toolId);
            }
        }
        const QString dockB64 = obj.value(QStringLiteral("dockState")).toString();
        if (!dockB64.isEmpty()) {
            session.dockState = QByteArray::fromBase64(dockB64.toLatin1());
        }
        state.sessions.append(session);
    }

    if (ok) {
        *ok = true;
    }
    return state;
}

const WorkspaceSessionEntry *WorkspaceState::sessionFor(const QUuid &connectionId) const
{
    if (connectionId.isNull()) {
        return nullptr;
    }
    for (const WorkspaceSessionEntry &session : sessions) {
        if (session.connectionId == connectionId) {
            return &session;
        }
    }
    return nullptr;
}
