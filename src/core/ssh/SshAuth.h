/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"

#include <QString>

#include <libssh/libssh.h>

/**
 * SSH user authentication helpers (password, keyboard-interactive, agent, keys).
 */
class SshAuth
{
public:
    /// Authenticate `session` using `profile` and mutable `secret` (password/passphrase).
    /// On failure, optionally writes a short reason to `detailOut`.
    static bool authenticateSession(ssh_session session,
                                    const Connection &profile,
                                    QString secret,
                                    QString *detailOut = nullptr);

private:
    static bool authenticatePassword(ssh_session session, const QString &password);
    static bool authenticateKeyboardInteractive(ssh_session session, const QString &password);
    static bool authenticateWithAgent(ssh_session session);
    static bool authenticatePrivateKey(ssh_session session,
                                       const QString &keyPath,
                                       const QString &passphrase,
                                       QString *detailOut);
    static bool authenticatePublicKeyAuto(ssh_session session, const QString &passphrase);
};
