/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"

#include <QString>

#include <functional>
#include <vector>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>

#if defined(LIBSSH_VERSION_INT) && (LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 11, 0))
#error "easy-ssh requires libssh >= 0.11 for ProxyJump (SSH_OPTIONS_PROXYJUMP)"
#endif

/**
 * Owns ssh_session: connect options, ProxyJump, auth, keepalive.
 * Interactive shell lives in core/shell/SshShell. Host-key UI via HostKeyVerifyFn.
 */
class SshSession
{
public:
    using HostKeyVerifyFn = std::function<bool(ssh_session session, const QString &contextLabel)>;
    /// Return true to abort an in-flight establish()/ssh_connect loop.
    using CancelFn = std::function<bool()>;

    SshSession() = default;
    ~SshSession();

    SshSession(const SshSession &) = delete;
    SshSession &operator=(const SshSession &) = delete;

    void setHostKeyVerifier(HostKeyVerifyFn verifier);
    void setCancelChecker(CancelFn checker);

    ssh_session handle() const { return m_session; }
    bool isConnected() const;

    /// Create session, apply options, connect (+jump), verify host key, authenticate.
    /// Polls in short slices so @ref setCancelChecker can abort a slow connect.
    bool establish(const Connection &connection,
                   const SessionCredentials &credentials,
                   QString *errorOut);

    void cleanup();

    /// Returns false if keepalive failed and missed count exceeded max.
    bool pollKeepAlive(bool hadChannelActivity, QString *errorOut);

    void configureKeepAlive(int intervalSec, int countMax);
    void resetKeepAliveClock();

    QString sessionError() const;
    static QString sessionErrorOf(ssh_session session);

private:
    struct JumpHopContext
    {
        SshSession *session = nullptr;
        int hopIndex = 0;
    };

    bool applyConnectionOptions(const Connection &connection);
    void applyAdvancedOptions(const Connection &connection);
    void registerJumpCallbacks(const Connection &connection);
    bool connectWithFallback(const Connection &connection, QString *errorOut);
    /// Non-blocking ssh_connect loop with cancel checks (restores blocking on success).
    bool connectCancellable(QString *errorOut);
    bool isCancelRequested() const;
    void applyWindowsAlgorithmFallback();
    void logSessionOptions(const char *stage) const;

    int handleJumpBeforeConnection(ssh_session session, int hopIndex);
    int handleJumpVerifyKnownHost(ssh_session session, int hopIndex);
    int handleJumpAuthenticate(ssh_session session, int hopIndex);

    static int jumpBeforeConnectionCb(ssh_session session, void *userdata);
    static int jumpVerifyKnownHostCb(ssh_session session, void *userdata);
    static int jumpAuthenticateCb(ssh_session session, void *userdata);

    ssh_session m_session = nullptr;

    Connection m_connection;
    SessionCredentials m_credentials;
    HostKeyVerifyFn m_hostKeyVerifier;
    CancelFn m_cancelChecker;

    std::vector<JumpHopContext> m_jumpContexts;
    std::vector<ssh_jump_callbacks_struct> m_jumpCallbacks;

    int m_keepAliveIntervalSec = 0;
    int m_keepAliveCountMax = 3;
    qint64 m_lastKeepAliveMs = 0;
    int m_keepAliveMissed = 0;
};
