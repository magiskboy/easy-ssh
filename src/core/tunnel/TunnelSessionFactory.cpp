// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ITunnelSession.h"

#include "DynamicTunnelSession.h"
#include "LocalTunnelSession.h"
#include "RemoteTunnelSession.h"

ITunnelSession *
createTunnelSession(const TunnelDefinition &def, ssh_session session, QObject *parent)
{
    switch (def.type) {
    case TunnelType::Local:
        return new LocalTunnelSession(def, session, parent);
    case TunnelType::Remote:
        return new RemoteTunnelSession(def, session, parent);
    case TunnelType::Dynamic:
        return new DynamicTunnelSession(def, session, parent);
    }
    return new DynamicTunnelSession(def, session, parent);
}
