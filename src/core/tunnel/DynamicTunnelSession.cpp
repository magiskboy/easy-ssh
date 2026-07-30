#include "DynamicTunnelSession.h"

DynamicTunnelSession::DynamicTunnelSession(const TunnelDefinition &def, QObject *parent)
    : ITunnelSession(parent), m_def(def)
{
}

bool DynamicTunnelSession::start()
{
    const QString message = tr("Dynamic (SOCKS) tunnels are not implemented yet");
    emit errorOccurred(m_def.id, message);
    emit statusChanged(m_def.id, QStringLiteral("Error"), message);
    return false;
}

void DynamicTunnelSession::stop(bool emitOff)
{
    if (emitOff) {
        emit statusChanged(m_def.id, QStringLiteral("Off"), QString());
    }
}
