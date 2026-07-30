/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>

#include <functional>

#include <libssh/libssh.h>

/**
 * Known-hosts verification and known_hosts file updates.
 * Blocking UI trust prompts are supplied by the caller (SshWorker) via TrustCallback.
 */
class SshKnownHosts
{
public:
    enum class Disposition
    {
        Unknown,
        Changed,
        Other,
    };

    /// Return true if the user accepted the host key.
    using TrustCallback = std::function<bool(
        Disposition disposition, const QString &fingerprint, const QString &contextLabel)>;
    using ErrorCallback = std::function<void(const QString &message)>;

    static QString fingerprintOf(ssh_session session);
    static QString knownHostsFilePathFor(ssh_session session);
    static bool knownHostsLineMatchesHost(const QString &hostField, const QString &host, int port);
    static bool removeKnownHostsEntriesForSession(ssh_session session);

    /// Verify server host key; may call trustCallback for unknown/changed keys.
    /// onError is invoked for hard failures when contextLabel is empty (direct connect).
    static bool verify(ssh_session session,
                       const QString &contextLabel,
                       const TrustCallback &trustCallback,
                       const ErrorCallback &onError = {});
};
