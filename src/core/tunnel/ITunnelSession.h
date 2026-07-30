#pragma once

#include "Tunnel.h"

#include <QObject>
#include <QString>
#include <QUuid>

#include <libssh/libssh.h>

/**
 * Per-active-tunnel interface (parallel to FsEngine).
 * Concrete: LocalTunnelSession, RemoteTunnelSession; Dynamic — not implement yet.
 */
class ITunnelSession : public QObject
{
    Q_OBJECT

public:
    explicit ITunnelSession(QObject *parent = nullptr) : QObject(parent) {}
    ~ITunnelSession() override = default;

    virtual QUuid id() const = 0;
    virtual TunnelType type() const = 0;

    /// Bind listen / remote forward. On failure emit error + return false.
    virtual bool start() = 0;
    virtual void stop(bool emitOff) = 0;

    /// Poll bridges (and any per-session work). Remote accept is coordinated by the worker.
    virtual void poll() = 0;

signals:
    void statusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void errorOccurred(const QUuid &tunnelId, const QString &message);
};

/// Create Local/Remote session (parented). Dynamic → stub that reports NotSupported.
ITunnelSession *
createTunnelSession(const TunnelDefinition &def, ssh_session session, QObject *parent);
