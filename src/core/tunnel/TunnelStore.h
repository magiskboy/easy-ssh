/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Tunnel.h"

#include <QList>
#include <QUuid>

class TunnelStore
{
public:
    static QList<TunnelDefinition> load();
    static void save(const QList<TunnelDefinition> &tunnels);

    static QList<TunnelDefinition> loadForConnection(const QUuid &connectionId);
    static void removeByConnectionId(const QUuid &connectionId);
};
