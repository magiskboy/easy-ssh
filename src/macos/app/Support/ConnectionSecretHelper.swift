// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

enum ConnectionSecretHelper {
    /// Persist target secrets for a connection (Phase 2: password / passphrase only).
    static func persistSecrets(
        for connection: ESSConnectionInfo,
        previousAuthType: ESSAuthType?,
        isEdit: Bool,
        password: String?,
        passphrase: String?
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
    }

    /// Load the target secret (password or passphrase) for connect.
    static func loadTargetSecret(
        for connection: ESSConnectionInfo,
        completion: @escaping (String?) -> Void
    ) {
        let kind: ESSSecretKind = connection.authType == .password ? .password : .passphrase
        ESSSecretStore.shared().readSecret(for: connection.connectionId, kind: kind) { value, ok, _ in
            completion(ok ? value : nil)
        }
    }
}
