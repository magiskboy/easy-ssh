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
