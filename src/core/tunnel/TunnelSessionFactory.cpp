// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ITunnelSession.h"

#include "DynamicTunnelSession.h"
#include "LocalTunnelSession.h"
#include "RemoteTunnelSession.h"

ITunnelSession *createTunnelSession(const TunnelDefinition &def,
                                    ssh_session session,
                                    SshIoLoop *loop,
                                    QObject *parent)
{
    switch (def.type) {
    case TunnelType::Local:
        return new LocalTunnelSession(def, session, loop, parent);
    case TunnelType::Remote:
        return new RemoteTunnelSession(def, session, loop, parent);
    case TunnelType::Dynamic:
        return new DynamicTunnelSession(def, session, loop, parent);
    }
    return new DynamicTunnelSession(def, session, loop, parent);
}
