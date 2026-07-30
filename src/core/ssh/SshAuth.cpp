#include "SshAuth.h"

#include <QFile>

bool SshAuth::authenticateSession(ssh_session session, const Connection &profile, QString secret)
{
    if (session == nullptr) {
        return false;
    }

    int rc = ssh_userauth_none(session, nullptr);
    if (rc == SSH_AUTH_ERROR) {
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
            if (authenticatePrivateKey(session, profile.privateKeyPath, secret)) {
                return true;
            }
            return false;
        }

        if (authenticatePublicKeyAuto(session, secret)) {
            return true;
        }

        return false;
    }
    case AuthType::Password:
    default: {
        if (secret.isEmpty()) {
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
        }

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
                                     const QString &passphrase)
{
    if (keyPath.isEmpty()) {
        return false;
    }

    ssh_key key = nullptr;
    const QByteArray path = keyPath.toUtf8();
    const QByteArray phrase = passphrase.toUtf8();
    const char *phrasePtr = passphrase.isEmpty() ? nullptr : phrase.constData();

    int rc = ssh_pki_import_privkey_file(path.constData(), phrasePtr, nullptr, nullptr, &key);
    if (rc != SSH_OK) {
        return false;
    }

    rc = ssh_userauth_publickey(session, nullptr, key);
    ssh_key_free(key);

    return rc == SSH_AUTH_SUCCESS;
}

bool SshAuth::authenticatePublicKeyAuto(ssh_session session, const QString &passphrase)
{
    const QByteArray phrase = passphrase.toUtf8();
    const char *phrasePtr = passphrase.isEmpty() ? nullptr : phrase.constData();
    const int rc = ssh_userauth_publickey_auto(session, nullptr, phrasePtr);
    return rc == SSH_AUTH_SUCCESS;
}
