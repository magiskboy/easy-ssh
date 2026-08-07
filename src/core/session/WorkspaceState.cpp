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
        if (!session.activeTerminalId.isNull()) {
            obj.insert(QStringLiteral("activeTerminalId"), uuidToString(session.activeTerminalId));
        }
        if (!session.activeToolId.isEmpty()) {
            obj.insert(QStringLiteral("activeToolId"), session.activeToolId);
        }
        QJsonArray terminalsJson;
        for (const WorkspaceTerminalEntry &shell : session.terminals) {
            if (shell.id.isNull()) {
                continue;
            }
            QJsonObject terminalObj;
            terminalObj.insert(QStringLiteral("id"), uuidToString(shell.id));
            if (!shell.title.isEmpty()) {
                terminalObj.insert(QStringLiteral("title"), shell.title);
            }
            terminalsJson.append(terminalObj);
        }
        obj.insert(QStringLiteral("terminals"), terminalsJson);
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
        session.activeTerminalId =
            uuidFromString(obj.value(QStringLiteral("activeTerminalId")).toString());
        session.activeToolId = obj.value(QStringLiteral("activeToolId")).toString();
        const QJsonArray terminalsJson = obj.value(QStringLiteral("terminals")).toArray();
        for (const QJsonValue &terminalValue : terminalsJson) {
            if (!terminalValue.isObject()) {
                continue;
            }
            const QJsonObject terminalObj = terminalValue.toObject();
            WorkspaceTerminalEntry shell;
            shell.id = uuidFromString(terminalObj.value(QStringLiteral("id")).toString());
            if (shell.id.isNull()) {
                continue;
            }
            shell.title = terminalObj.value(QStringLiteral("title")).toString();
            session.terminals.append(shell);
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
