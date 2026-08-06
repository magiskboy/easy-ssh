// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

/// Editable Phase 2 session fields (+ ephemeral secrets for the form).
struct ConnectionFormState: Equatable {
    var connectionId: UUID = UUID()
    var name: String = ""
    var host: String = ""
    var port: Int = 22
    var username: String = NSUserName()
    var usePrivateKey: Bool = false
    var savePassword: Bool = false
    var privateKeyPath: String = ""
    var source: ESSConnectionSource = .app
    var configAlias: String = ""

    /// Ephemeral — not persisted on Connection.
    var password: String = ""
    var passphrase: String = ""

    var displayName: String {
        if !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return name
        }
        let user = username.isEmpty ? "user" : username
        let h = host.isEmpty ? "host" : host
        return "\(user)@\(h):\(port)"
    }

    var isValid: Bool {
        !host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
            !username.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
            port > 0 && port <= 65535 &&
            (usePrivateKey ? !privateKeyPath.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty : true)
    }

    var isReadOnly: Bool { source == .sshConfig }

    static func from(_ info: ESSConnectionInfo) -> ConnectionFormState {
        ConnectionFormState(
            connectionId: info.connectionId as UUID,
            name: info.name,
            host: info.host,
            port: Int(info.port),
            username: info.username,
            usePrivateKey: info.authType == .privateKey,
            savePassword: info.savePassword,
            privateKeyPath: info.privateKeyPath ?? "",
            source: info.source,
            configAlias: info.configAlias,
            password: "",
            passphrase: ""
        )
    }

    static func from(draft: ConnectionDraft) -> ConnectionFormState {
        ConnectionFormState(
            connectionId: UUID(),
            name: draft.name.isEmpty ? draft.displayName : draft.name,
            host: draft.host,
            port: draft.port,
            username: draft.username,
            usePrivateKey: draft.usePrivateKey,
            savePassword: draft.savePassword,
            privateKeyPath: draft.privateKeyPath,
            source: .app,
            configAlias: "",
            password: draft.password,
            passphrase: draft.passphrase
        )
    }

    /// Builds a new ESSConnectionInfo, optionally merging advanced fields from an existing row.
    func makeConnectionInfo(preservingAdvancedFrom existing: ESSConnectionInfo? = nil) -> ESSConnectionInfo {
        let info: ESSConnectionInfo
        if let existing {
            info = essCopyConnectionInfo(existing)
        } else {
            info = ESSConnectionInfo()
        }
        info.connectionId = connectionId
        let trimmedName = name.trimmingCharacters(in: .whitespacesAndNewlines)
        info.name = trimmedName.isEmpty ? displayName : trimmedName
        info.host = host.trimmingCharacters(in: .whitespacesAndNewlines)
        info.port = port
        info.username = username.trimmingCharacters(in: .whitespacesAndNewlines)
        info.authType = usePrivateKey ? .privateKey : .password
        info.savePassword = savePassword
        let key = privateKeyPath.trimmingCharacters(in: .whitespacesAndNewlines)
        info.privateKeyPath = key.isEmpty ? nil : key
        info.source = .app
        info.configAlias = ""
        return info
    }

    func makeCredentials() -> ESSSessionCredentials {
        let creds = ESSSessionCredentials()
        if usePrivateKey {
            creds.targetSecret = passphrase.isEmpty ? nil : passphrase
        } else {
            creds.targetSecret = password.isEmpty ? nil : password
        }
        return creds
    }
}
