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
        Q_UNUSED(session);
        return new DynamicTunnelSession(def, parent);
    }
    Q_UNUSED(session);
    return new DynamicTunnelSession(def, parent);
}
