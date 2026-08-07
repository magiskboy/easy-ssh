/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

struct WorkspaceTerminalEntry
{
    QUuid id;
    QString title;
};

struct WorkspaceSessionEntry
{
    QUuid connectionId;
    QUuid activeTerminalId;
    QString activeToolId;
    QList<WorkspaceTerminalEntry> terminals;
    /// Open explorer/tool dock ids (process, container, service, systeminfo).
    QStringList tools;
    QByteArray dockState;
};

struct WorkspaceState
{
    static constexpr int kCurrentVersion = 1;

    int version = kCurrentVersion;
    QUuid activeConnectionId;
    QList<WorkspaceSessionEntry> sessions;

    bool isEmpty() const { return sessions.isEmpty(); }

    QByteArray toJson() const;
    static WorkspaceState fromJson(const QByteArray &json, bool *ok = nullptr);

    const WorkspaceSessionEntry *sessionFor(const QUuid &connectionId) const;
};
