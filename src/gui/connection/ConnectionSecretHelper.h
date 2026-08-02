/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"

class SecretStore;

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
                    bool gatewayPassphraseProvided);

} // namespace ConnectionSecretHelper
