// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

struct JumpHopForm: Equatable, Identifiable {
    var id: UUID = UUID()
    var host: String = ""
    var port: Int = 22
    var username: String = ""
    var usePrivateKey: Bool = false
    var privateKeyPath: String = ""
    var useTargetCredentials: Bool = true

    /// Identity is UI-only; dirty tracking compares content fields.
    static func == (lhs: JumpHopForm, rhs: JumpHopForm) -> Bool {
        lhs.host == rhs.host &&
            lhs.port == rhs.port &&
            lhs.username == rhs.username &&
            lhs.usePrivateKey == rhs.usePrivateKey &&
            lhs.privateKeyPath == rhs.privateKeyPath &&
            lhs.useTargetCredentials == rhs.useTargetCredentials
    }

    var displayLabel: String {
        let user = username.trimmingCharacters(in: .whitespacesAndNewlines)
        let h = host.trimmingCharacters(in: .whitespacesAndNewlines)
        if user.isEmpty, h.isEmpty { return "New hop" }
        let base = "\(user.isEmpty ? "user" : user)@\(h.isEmpty ? "host" : h)"
        return port == 22 ? base : "\(base):\(port)"
    }

    static func from(_ hop: ESSJumpHop) -> JumpHopForm {
        JumpHopForm(
            host: hop.host,
            port: Int(hop.port),
            username: hop.username,
            usePrivateKey: hop.authType == .privateKey,
            privateKeyPath: hop.privateKeyPath ?? "",
            useTargetCredentials: hop.useTargetCredentials
        )
    }

    func makeESSJumpHop() -> ESSJumpHop {
        let hop = ESSJumpHop()
        hop.host = host.trimmingCharacters(in: .whitespacesAndNewlines)
        hop.port = port
        hop.username = username.trimmingCharacters(in: .whitespacesAndNewlines)
        hop.authType = usePrivateKey ? .privateKey : .password
        let key = privateKeyPath.trimmingCharacters(in: .whitespacesAndNewlines)
        hop.privateKeyPath = key.isEmpty ? nil : key
        hop.useTargetCredentials = useTargetCredentials
        return hop
    }
}

struct ShellCommandForm: Equatable {
    var shell: String = ""
    var listingCommand: String = ""
    var listFileCommand: String = ""
    var mkdirCommand: String = ""
    var removeCommand: String = ""
    var renameCommand: String = ""
    var pwdCommand: String = ""
    var realpathCommand: String = ""
    var symlinkCommand: String = ""
    var readlinkCommand: String = ""
    var clearAliases: Bool = true
    var clearNationalVars: Bool = true
    var tryFullTime: Bool = true
    var ignoreLsWarnings: Bool = false
    var allowScpFallback: Bool = true

    static func defaults() -> ShellCommandForm {
        ShellCommandForm()
    }

    static func from(_ set: ESSShellCommandSet) -> ShellCommandForm {
        ShellCommandForm(
            shell: set.shell,
            listingCommand: set.listingCommand,
            listFileCommand: set.listFileCommand,
            mkdirCommand: set.mkdirCommand,
            removeCommand: set.removeCommand,
            renameCommand: set.renameCommand,
            pwdCommand: set.pwdCommand,
            realpathCommand: set.realpathCommand,
            symlinkCommand: set.symlinkCommand,
            readlinkCommand: set.readlinkCommand,
            clearAliases: set.clearAliases,
            clearNationalVars: set.clearNationalVars,
            tryFullTime: set.tryFullTime,
            ignoreLsWarnings: set.ignoreLsWarnings,
            allowScpFallback: set.allowScpFallback
        )
    }

    func makeESSShellCommandSet() -> ESSShellCommandSet {
        let set = ESSShellCommandSet()
        set.shell = shell
        set.listingCommand = listingCommand
        set.listFileCommand = listFileCommand
        set.mkdirCommand = mkdirCommand
        set.removeCommand = removeCommand
        set.renameCommand = renameCommand
        set.pwdCommand = pwdCommand
        set.realpathCommand = realpathCommand
        set.symlinkCommand = symlinkCommand
        set.readlinkCommand = readlinkCommand
        set.clearAliases = clearAliases
        set.clearNationalVars = clearNationalVars
        set.tryFullTime = tryFullTime
        set.ignoreLsWarnings = ignoreLsWarnings
        set.allowScpFallback = allowScpFallback
        return set
    }
}

/// Editable connection fields (+ ephemeral secrets for the form).
struct ConnectionFormState: Equatable {
    var connectionId: UUID = UUID()
    var name: String = ""
    var host: String = ""
    var port: Int = 22
    var username: String = NSUserName()
    var usePrivateKey: Bool = false
    var savePassword: Bool = false
    var privateKeyPath: String = ""
    var startupDirectory: String = ""
    var agentForwarding: Bool = false
    var source: ESSConnectionSource = .app
    var configAlias: String = ""

    var keepAliveIntervalSec: Int = 0
    var keepAliveCountMax: Int = 3
    var compressionEnabled: Bool = false

    var proxyMode: ESSProxyMode = .none
    var jumpHops: [JumpHopForm] = [JumpHopForm()]
    /// UI selection only — not compared for dirty state.
    var selectedHopIndex: Int = 0
    var proxyCommand: String = ""

    var shellCommands: ShellCommandForm = .defaults()

    /// Ephemeral — not persisted on Connection.
    var password: String = ""
    var passphrase: String = ""
    var gatewayPassword: String = ""
    var gatewayPassphrase: String = ""

    /// Whether savePassword was already on when loading an existing row (edit blank-password OK).
    var initialSavePassword: Bool = false

    static func == (lhs: ConnectionFormState, rhs: ConnectionFormState) -> Bool {
        lhs.connectionId == rhs.connectionId &&
            lhs.name == rhs.name &&
            lhs.host == rhs.host &&
            lhs.port == rhs.port &&
            lhs.username == rhs.username &&
            lhs.usePrivateKey == rhs.usePrivateKey &&
            lhs.savePassword == rhs.savePassword &&
            lhs.privateKeyPath == rhs.privateKeyPath &&
            lhs.startupDirectory == rhs.startupDirectory &&
            lhs.agentForwarding == rhs.agentForwarding &&
            lhs.source == rhs.source &&
            lhs.configAlias == rhs.configAlias &&
            lhs.keepAliveIntervalSec == rhs.keepAliveIntervalSec &&
            lhs.keepAliveCountMax == rhs.keepAliveCountMax &&
            lhs.compressionEnabled == rhs.compressionEnabled &&
            lhs.proxyMode == rhs.proxyMode &&
            lhs.jumpHops == rhs.jumpHops &&
            lhs.proxyCommand == rhs.proxyCommand &&
            lhs.shellCommands == rhs.shellCommands &&
            lhs.password == rhs.password &&
            lhs.passphrase == rhs.passphrase &&
            lhs.gatewayPassword == rhs.gatewayPassword &&
            lhs.gatewayPassphrase == rhs.gatewayPassphrase &&
            lhs.initialSavePassword == rhs.initialSavePassword
    }

    var displayName: String {
        if !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return name
        }
        let user = username.isEmpty ? "user" : username
        let h = host.isEmpty ? "host" : host
        return "\(user)@\(h):\(port)"
    }

    var isReadOnly: Bool { source == .sshConfig }

    var usesCustomGateway: Bool {
        guard proxyMode == .proxyJump,
              let hop = jumpHops.first
        else { return false }
        return !hop.useTargetCredentials
    }

    var isValid: Bool {
        validationError(isCreate: false) == nil
    }

    func isValid(isCreate: Bool) -> Bool {
        validationError(isCreate: isCreate) == nil
    }

    func validationError(isCreate: Bool) -> String? {
        if host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return "Host is required."
        }
        if username.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return "Username is required."
        }
        if port < 1 || port > 65535 {
            return "Port must be between 1 and 65535."
        }
        if usePrivateKey {
            // Key path optional — empty uses ssh-agent / default identities (Qt parity).
        } else if savePassword, password.isEmpty, isCreate || !initialSavePassword {
            return "Password is required when saving to the Keychain."
        }

        switch proxyMode {
        case .proxyJump:
            if jumpHops.isEmpty {
                return "Add at least one ProxyJump hop."
            }
            for (i, hop) in jumpHops.enumerated() {
                if hop.host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    return "Gateway host is required for hop \(i + 1)."
                }
                if hop.username.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    return "Gateway username is required for hop \(i + 1)."
                }
                if i == 0, !hop.useTargetCredentials, !hop.usePrivateKey, isCreate,
                   gatewayPassword.isEmpty
                {
                    return "Gateway password is required."
                }
            }
        case .proxyCommand:
            let command = proxyCommand.trimmingCharacters(in: .whitespacesAndNewlines)
            if command.isEmpty || Self.isSshNoneToken(command) {
                return "ProxyCommand is required."
            }
        case .none:
            break
        @unknown default:
            break
        }
        return nil
    }

    static func from(_ info: ESSConnectionInfo) -> ConnectionFormState {
        let hops = info.jumpHops.map { JumpHopForm.from($0) }
        return ConnectionFormState(
            connectionId: info.connectionId as UUID,
            name: info.name,
            host: info.host,
            port: Int(info.port),
            username: info.username,
            usePrivateKey: info.authType == .privateKey,
            savePassword: info.savePassword,
            privateKeyPath: info.privateKeyPath ?? "",
            startupDirectory: info.startupDirectory,
            agentForwarding: info.agentForwarding,
            source: info.source,
            configAlias: info.configAlias,
            keepAliveIntervalSec: Int(info.keepAliveIntervalSec),
            keepAliveCountMax: Int(info.keepAliveCountMax),
            compressionEnabled: info.compressionEnabled,
            proxyMode: info.proxyMode,
            jumpHops: hops.isEmpty ? [JumpHopForm()] : hops,
            selectedHopIndex: 0,
            proxyCommand: info.proxyCommand,
            shellCommands: ShellCommandForm.from(info.shellCommands),
            password: "",
            passphrase: "",
            gatewayPassword: "",
            gatewayPassphrase: "",
            initialSavePassword: info.savePassword
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
            passphrase: draft.passphrase,
            initialSavePassword: false
        )
    }

    /// Builds ESSConnectionInfo from all form fields (advanced included).
    func makeConnectionInfo(preservingAdvancedFrom existing: ESSConnectionInfo? = nil) -> ESSConnectionInfo {
        _ = existing // retained for call-site compatibility; form always writes advanced fields
        var normalized = self
        normalized.normalizeProxyFields()

        let info = ESSConnectionInfo()
        info.connectionId = normalized.connectionId
        let trimmedName = normalized.name.trimmingCharacters(in: .whitespacesAndNewlines)
        info.name = trimmedName.isEmpty ? normalized.displayName : trimmedName
        info.host = normalized.host.trimmingCharacters(in: .whitespacesAndNewlines)
        info.port = normalized.port
        info.username = normalized.username.trimmingCharacters(in: .whitespacesAndNewlines)
        info.authType = normalized.usePrivateKey ? .privateKey : .password
        info.savePassword = normalized.savePassword
        let key = normalized.privateKeyPath.trimmingCharacters(in: .whitespacesAndNewlines)
        info.privateKeyPath = key.isEmpty ? nil : key
        info.startupDirectory = normalized.startupDirectory.trimmingCharacters(in: .whitespacesAndNewlines)
        info.source = .app
        info.configAlias = ""
        info.proxyMode = normalized.proxyMode
        info.keepAliveIntervalSec = normalized.keepAliveIntervalSec
        info.keepAliveCountMax = max(1, normalized.keepAliveCountMax)
        info.compressionEnabled = normalized.compressionEnabled
        info.agentForwarding = normalized.agentForwarding
        info.shellCommands = normalized.shellCommands.makeESSShellCommandSet()

        switch normalized.proxyMode {
        case .proxyJump:
            info.jumpHops = normalized.jumpHops.map { $0.makeESSJumpHop() }
            info.proxyCommand = ""
        case .proxyCommand:
            info.jumpHops = []
            info.proxyCommand = normalized.proxyCommand.trimmingCharacters(in: .whitespacesAndNewlines)
        case .none:
            info.jumpHops = []
            info.proxyCommand = ""
        @unknown default:
            info.jumpHops = []
            info.proxyCommand = ""
        }
        return info
    }

    func makeCredentials() -> ESSSessionCredentials {
        let creds = ESSSessionCredentials()
        if usePrivateKey {
            creds.targetSecret = passphrase.isEmpty ? nil : passphrase
        } else {
            creds.targetSecret = password.isEmpty ? nil : password
        }
        if usesCustomGateway {
            let hop = jumpHops[0]
            if hop.usePrivateKey {
                creds.gatewaySecret = gatewayPassphrase.isEmpty ? nil : gatewayPassphrase
            } else {
                creds.gatewaySecret = gatewayPassword.isEmpty ? nil : gatewayPassword
            }
        }
        return creds
    }

    mutating func normalizeProxyFields() {
        let command = proxyCommand.trimmingCharacters(in: .whitespacesAndNewlines)
        if Self.isSshNoneToken(command) {
            proxyCommand = ""
            if proxyMode == .proxyCommand {
                proxyMode = .none
            }
        }

        switch proxyMode {
        case .none:
            jumpHops = [JumpHopForm()]
            selectedHopIndex = 0
            proxyCommand = ""
        case .proxyJump:
            proxyCommand = ""
            if jumpHops.isEmpty {
                jumpHops = [JumpHopForm()]
            }
            selectedHopIndex = min(max(0, selectedHopIndex), jumpHops.count - 1)
        case .proxyCommand:
            jumpHops = [JumpHopForm()]
            selectedHopIndex = 0
        @unknown default:
            break
        }
    }

    mutating func addHop() {
        jumpHops.append(JumpHopForm())
        selectedHopIndex = jumpHops.count - 1
        proxyMode = .proxyJump
    }

    mutating func removeSelectedHop() {
        guard jumpHops.count > 1, jumpHops.indices.contains(selectedHopIndex) else { return }
        jumpHops.remove(at: selectedHopIndex)
        selectedHopIndex = min(selectedHopIndex, jumpHops.count - 1)
    }

    mutating func selectHop(at index: Int) {
        guard jumpHops.indices.contains(index) else { return }
        selectedHopIndex = index
    }

    var selectedHop: JumpHopForm? {
        get {
            guard jumpHops.indices.contains(selectedHopIndex) else { return nil }
            return jumpHops[selectedHopIndex]
        }
        set {
            guard let newValue, jumpHops.indices.contains(selectedHopIndex) else { return }
            jumpHops[selectedHopIndex] = newValue
        }
    }

    private static func isSshNoneToken(_ value: String) -> Bool {
        value.trimmingCharacters(in: .whitespacesAndNewlines)
            .caseInsensitiveCompare("none") == .orderedSame
    }
}
