// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionSecretHelper.h"

#include "core/connection/SecretStore.h"

namespace ConnectionSecretHelper
{

void persistSecrets(SecretStore *secretStore,
                    const Connection &connection,
                    AuthType previousAuthType,
                    bool isEdit,
                    const QString &password,
                    bool passwordProvided,
                    const QString &passphrase,
                    bool passphraseProvided,
                    const QString &gatewayPassword,
                    bool gatewayPasswordProvided,
                    const QString &gatewayPassphrase,
                    bool gatewayPassphraseProvided)
{
    if (!secretStore) {
        return;
    }

    if (isEdit && previousAuthType != connection.authType) {
        if (previousAuthType == AuthType::Password) {
            secretStore->deleteSecret(connection.id, SecretStore::Kind::Password);
        } else {
            secretStore->deleteSecret(connection.id, SecretStore::Kind::Passphrase);
        }
    }

    if (connection.authType == AuthType::Password) {
        if (passwordProvided) {
            secretStore->storeSecret(connection.id, SecretStore::Kind::Password, password);
        }
        if (isEdit) {
            secretStore->deleteSecret(connection.id, SecretStore::Kind::Passphrase);
        }
    } else {
        if (passphraseProvided) {
            secretStore->storeSecret(connection.id, SecretStore::Kind::Passphrase, passphrase);
        }
        if (isEdit) {
            secretStore->deleteSecret(connection.id, SecretStore::Kind::Password);
        }
    }

    const bool usesCustomGateway =
        connection.usesJumpHost() && !connection.jumpHops.first().useTargetCredentials;
    if (!usesCustomGateway) {
        secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassword);
        secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassphrase);
        return;
    }

    const AuthType gatewayAuth = connection.jumpHops.first().authType;
    if (gatewayAuth == AuthType::Password) {
        if (gatewayPasswordProvided) {
            secretStore->storeSecret(
                connection.id, SecretStore::Kind::GatewayPassword, gatewayPassword);
        }
        secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassphrase);
    } else {
        if (gatewayPassphraseProvided) {
            secretStore->storeSecret(
                connection.id, SecretStore::Kind::GatewayPassphrase, gatewayPassphrase);
        }
        secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassword);
    }
}

} // namespace ConnectionSecretHelper
