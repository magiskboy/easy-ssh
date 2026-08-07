// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

enum ConnectionSecretHelper {
    /// Persist target + gateway secrets (mirrors Qt ConnectionSecretHelper).
    static func persistSecrets(
        for connection: ESSConnectionInfo,
        previousAuthType: ESSAuthType?,
        isEdit: Bool,
        password: String?,
        passphrase: String?,
        gatewayPassword: String? = nil,
        gatewayPassphrase: String? = nil
    ) {
        let store = ESSSecretStore.shared()
        let id = connection.connectionId

        if isEdit, let previous = previousAuthType, previous != connection.authType {
            let kind: ESSSecretKind = previous == .password ? .password : .passphrase
            store.deleteSecret(for: id, kind: kind) { _, _ in }
        }

        if connection.authType == .password {
            if connection.savePassword, let password, !password.isEmpty {
                store.storeSecret(for: id, kind: .password, value: password) { _, _ in }
            } else if !connection.savePassword {
                store.deleteSecret(for: id, kind: .password) { _, _ in }
            }
            if isEdit {
                store.deleteSecret(for: id, kind: .passphrase) { _, _ in }
            }
        } else {
            if let passphrase, !passphrase.isEmpty {
                store.storeSecret(for: id, kind: .passphrase, value: passphrase) { _, _ in }
            }
            if isEdit {
                store.deleteSecret(for: id, kind: .password) { _, _ in }
            }
        }

        let usesCustomGateway =
            connection.proxyMode == .proxyJump
            && !(connection.jumpHops.first?.useTargetCredentials ?? true)

        if !usesCustomGateway {
            store.deleteSecret(for: id, kind: .gatewayPassword) { _, _ in }
            store.deleteSecret(for: id, kind: .gatewayPassphrase) { _, _ in }
            return
        }

        let gatewayAuth = connection.jumpHops.first?.authType ?? .password
        if gatewayAuth == .password {
            if let gatewayPassword, !gatewayPassword.isEmpty {
                store.storeSecret(for: id, kind: .gatewayPassword, value: gatewayPassword) { _, _ in }
            }
            store.deleteSecret(for: id, kind: .gatewayPassphrase) { _, _ in }
        } else {
            if let gatewayPassphrase, !gatewayPassphrase.isEmpty {
                store.storeSecret(for: id, kind: .gatewayPassphrase, value: gatewayPassphrase) {
                    _, _ in
                }
            }
            store.deleteSecret(for: id, kind: .gatewayPassword) { _, _ in }
        }
    }

    /// Load the target secret (password or passphrase) for connect.
    static func loadTargetSecret(
        for connection: ESSConnectionInfo,
        completion: @escaping (String?) -> Void
    ) {
        let kind: ESSSecretKind = connection.authType == .password ? .password : .passphrase
        ESSSecretStore.shared().readSecret(for: connection.connectionId, kind: kind) {
            value, ok, _ in
            completion(ok ? value : nil)
        }
    }

    /// Load gateway secret for hop 0 when ProxyJump uses custom credentials.
    static func loadGatewaySecret(
        for connection: ESSConnectionInfo,
        completion: @escaping (String?) -> Void
    ) {
        guard needsGatewaySecret(connection) else {
            completion(nil)
            return
        }
        let hop = connection.jumpHops[0]
        let kind: ESSSecretKind = hop.authType == .password ? .gatewayPassword : .gatewayPassphrase
        ESSSecretStore.shared().readSecret(for: connection.connectionId, kind: kind) {
            value, ok, _ in
            completion(ok ? value : nil)
        }
    }

    /// Load target + gateway secrets for connect.
    static func loadCredentials(
        for connection: ESSConnectionInfo,
        completion: @escaping (_ target: String?, _ gateway: String?) -> Void
    ) {
        loadTargetSecret(for: connection) { target in
            loadGatewaySecret(for: connection) { gateway in
                completion(target, gateway)
            }
        }
    }

    static func needsGatewaySecret(_ connection: ESSConnectionInfo) -> Bool {
        connection.proxyMode == .proxyJump
            && !connection.jumpHops.isEmpty
            && !connection.jumpHops[0].useTargetCredentials
    }
}
