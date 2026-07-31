// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SshAuth.h"

#include <QCoreApplication>

namespace
{
QString trAuth(const char *text)
{
    return QCoreApplication::translate("SshAuth", text);
}

void setDetail(QString *detailOut, const char *text)
{
    if (detailOut) {
        *detailOut = trAuth(text);
    }
}
} // namespace

bool SshAuth::authenticateSession(ssh_session session,
                                  const Connection &profile,
                                  QString secret,
                                  QString *detailOut)
{
    if (session == nullptr) {
        setDetail(detailOut, "Invalid SSH session");
        return false;
    }

    int rc = ssh_userauth_none(session, nullptr);
    if (rc == SSH_AUTH_ERROR) {
        setDetail(detailOut, "Authentication probe failed");
        return false;
    }
    if (rc == SSH_AUTH_SUCCESS) {
        return true;
    }

    switch (profile.authType) {
    case AuthType::PrivateKey: {
        if (authenticateWithAgent(session)) {
            return true;
        }

        if (!profile.privateKeyPath.isEmpty()) {
            if (authenticatePrivateKey(session, profile.privateKeyPath, secret, detailOut)) {
                return true;
            }
            return false;
        }

        if (authenticatePublicKeyAuto(session, secret)) {
            return true;
        }

        setDetail(detailOut, "Public key authentication failed (ssh-agent and default identities)");
        return false;
    }
    case AuthType::Password:
    default: {
        if (secret.isEmpty()) {
            setDetail(detailOut, "No password provided");
            return false;
        }

        if (authenticatePassword(session, secret)) {
            return true;
        }

        const int methods = ssh_userauth_list(session, nullptr);
        if (methods & SSH_AUTH_METHOD_INTERACTIVE) {
            if (authenticateKeyboardInteractive(session, secret)) {
                return true;
            }
            setDetail(detailOut, "Password and keyboard-interactive authentication failed");
            return false;
        }

        setDetail(detailOut, "Password authentication failed");
        return false;
    }
    }
}

bool SshAuth::authenticatePassword(ssh_session session, const QString &password)
{
    const QByteArray pwd = password.toUtf8();
    const int rc = ssh_userauth_password(session, nullptr, pwd.constData());
    return rc == SSH_AUTH_SUCCESS;
}

bool SshAuth::authenticateKeyboardInteractive(ssh_session session, const QString &password)
{
    int rc = ssh_userauth_kbdint(session, nullptr, nullptr);
    while (rc == SSH_AUTH_INFO) {
        const int nprompts = ssh_userauth_kbdint_getnprompts(session);
        for (int i = 0; i < nprompts; ++i) {
            char echo = 0;
            const char *prompt = ssh_userauth_kbdint_getprompt(session, i, &echo);
            Q_UNUSED(prompt);
            Q_UNUSED(echo);
            const QByteArray answer = password.toUtf8();
            if (ssh_userauth_kbdint_setanswer(session, i, answer.constData()) < 0) {
                return false;
            }
        }
        rc = ssh_userauth_kbdint(session, nullptr, nullptr);
    }
    return rc == SSH_AUTH_SUCCESS;
}

bool SshAuth::authenticateWithAgent(ssh_session session)
{
    const int methods = ssh_userauth_list(session, nullptr);
    if (!(methods & SSH_AUTH_METHOD_PUBLICKEY)) {
        return false;
    }

    const int rc = ssh_userauth_agent(session, nullptr);
    return rc == SSH_AUTH_SUCCESS;
}

bool SshAuth::authenticatePrivateKey(ssh_session session,
                                     const QString &keyPath,
                                     const QString &passphrase,
                                     QString *detailOut)
{
    if (keyPath.isEmpty()) {
        setDetail(detailOut, "Private key path is empty");
        return false;
    }

    ssh_key key = nullptr;
    const QByteArray path = keyPath.toUtf8();
    const QByteArray phrase = passphrase.toUtf8();
    const char *phrasePtr = passphrase.isEmpty() ? nullptr : phrase.constData();

    int rc = ssh_pki_import_privkey_file(path.constData(), phrasePtr, nullptr, nullptr, &key);
    if (rc != SSH_OK) {
        setDetail(detailOut, "Failed to import private key (check path and passphrase)");
        return false;
    }

    rc = ssh_userauth_publickey(session, nullptr, key);
    ssh_key_free(key);

    if (rc != SSH_AUTH_SUCCESS) {
        setDetail(detailOut, "Public key authentication failed");
        return false;
    }

    return true;
}

bool SshAuth::authenticatePublicKeyAuto(ssh_session session, const QString &passphrase)
{
    const QByteArray phrase = passphrase.toUtf8();
    const char *phrasePtr = passphrase.isEmpty() ? nullptr : phrase.constData();
    const int rc = ssh_userauth_publickey_auto(session, nullptr, phrasePtr);
    return rc == SSH_AUTH_SUCCESS;
}
