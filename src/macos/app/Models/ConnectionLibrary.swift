// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

enum ConnectionSourceFilter: String, CaseIterable, Identifiable {
    case all
    case app
    case sshConfig

    var id: String { rawValue }

    var title: String {
        switch self {
        case .all: return "All"
        case .app: return "Easy SSH"
        case .sshConfig: return "SSH Config"
        }
    }
}

@MainActor
final class ConnectionLibrary: ObservableObject {
    @Published private(set) var appConnections: [ESSConnectionInfo] = []
    @Published private(set) var sshConfigConnections: [ESSConnectionInfo] = []

    var allConnections: [ESSConnectionInfo] {
        appConnections + sshConfigConnections
    }

    init() {
        reload()
    }

    func reload() {
        reloadAppConnections()
        reloadSshConfig()
    }

    func reloadAppConnections() {
        appConnections = ESSConnectionStore.loadConnections()
    }

    func reloadSshConfig() {
        sshConfigConnections = ESSSshConfigParser.connections(fromConfigPath: nil)
    }

    func connection(id: UUID) -> ESSConnectionInfo? {
        allConnections.first { $0.connectionId as UUID == id }
    }

    func filtered(query: String, source: ConnectionSourceFilter) -> [ESSConnectionInfo] {
        let base: [ESSConnectionInfo]
        switch source {
        case .all: base = allConnections
        case .app: base = appConnections
        case .sshConfig: base = sshConfigConnections
        }

        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return base }

        let needle = trimmed.lowercased()
        return base.filter { info in
            matches(info, needle: needle)
        }
    }

    @discardableResult
    func add(_ info: ESSConnectionInfo) -> Bool {
        guard info.source == .app else { return false }
        appConnections.append(info)
        return persistAppConnections()
    }

    @discardableResult
    func update(_ info: ESSConnectionInfo) -> Bool {
        guard info.source == .app,
              let idx = appConnections.firstIndex(where: { $0.connectionId as UUID == info.connectionId as UUID })
        else { return false }
        appConnections[idx] = info
        return persistAppConnections()
    }

    @discardableResult
    func delete(id: UUID) -> Bool {
        guard let idx = appConnections.firstIndex(where: { $0.connectionId as UUID == id }) else {
            return false
        }
        let removed = appConnections.remove(at: idx)
        guard persistAppConnections() else { return false }
        ESSSecretStore.shared().deleteAllSecrets(forConnectionId: removed.connectionId) { _, _ in }
        return true
    }

    @discardableResult
    func duplicate(id: UUID) -> ESSConnectionInfo? {
        guard let source = appConnections.first(where: { $0.connectionId as UUID == id }),
              source.source == .app
        else { return nil }

        let copy = essCopyConnectionInfo(source)
        copy.connectionId = UUID()
        let baseName = source.name.isEmpty ? source.displayText : source.name
        copy.name = "\(baseName) (copy)"
        copy.source = .app
        copy.configAlias = ""
        guard add(copy) else { return nil }

        let kinds: [ESSSecretKind] = [.password, .passphrase, .gatewayPassword, .gatewayPassphrase]
        let store = ESSSecretStore.shared()
        for kind in kinds {
            store.copySecret(from: source.connectionId, to: copy.connectionId, kind: kind) { _, _ in }
        }
        return copy
    }

    @discardableResult
    func importFromSshConfig(id: UUID) -> ESSConnectionInfo? {
        guard let source = sshConfigConnections.first(where: { $0.connectionId as UUID == id }),
              source.source == .sshConfig
        else { return nil }

        let copy = essCopyConnectionInfo(source)
        copy.connectionId = UUID()
        copy.source = .app
        copy.configAlias = ""
        guard add(copy) else { return nil }
        return copy
    }

    func recentConnections(limit: Int = 8) -> [ESSConnectionInfo] {
        let ids = ESSAppSettings.shared().recentConnectionIds(withLimit: limit)
        return ids.compactMap { id in connection(id: id as UUID) }
    }

    // MARK: - Private

    @discardableResult
    private func persistAppConnections() -> Bool {
        do {
            try ESSConnectionStore.saveConnections(appConnections)
            return true
        } catch {
            reloadAppConnections()
            return false
        }
    }

    private func matches(_ info: ESSConnectionInfo, needle: String) -> Bool {
        let fields = [
            info.name,
            info.host,
            info.username,
            info.configAlias,
            String(info.port),
            info.displayText,
        ]
        return fields.contains { $0.lowercased().contains(needle) }
    }

    static func copyInfo(_ source: ESSConnectionInfo) -> ESSConnectionInfo {
        essCopyConnectionInfo(source)
    }
}

func essCopyConnectionInfo(_ source: ESSConnectionInfo) -> ESSConnectionInfo {
    let copy = ESSConnectionInfo()
    copy.connectionId = source.connectionId
    copy.name = source.name
    copy.host = source.host
    copy.port = source.port
    copy.username = source.username
    copy.authType = source.authType
    copy.savePassword = source.savePassword
    copy.privateKeyPath = source.privateKeyPath
    copy.startupDirectory = source.startupDirectory
    copy.source = source.source
    copy.configAlias = source.configAlias
    copy.proxyMode = source.proxyMode
    copy.proxyCommand = source.proxyCommand
    copy.keepAliveIntervalSec = source.keepAliveIntervalSec
    copy.keepAliveCountMax = source.keepAliveCountMax
    copy.compressionEnabled = source.compressionEnabled
    copy.agentForwarding = source.agentForwarding
    copy.displayText = source.displayText

    copy.jumpHops = source.jumpHops.map { hop in
        let h = ESSJumpHop()
        h.host = hop.host
        h.port = hop.port
        h.username = hop.username
        h.authType = hop.authType
        h.privateKeyPath = hop.privateKeyPath
        h.useTargetCredentials = hop.useTargetCredentials
        return h
    }

    let shell = ESSShellCommandSet()
    shell.shell = source.shellCommands.shell
    shell.listingCommand = source.shellCommands.listingCommand
    shell.listFileCommand = source.shellCommands.listFileCommand
    shell.mkdirCommand = source.shellCommands.mkdirCommand
    shell.removeCommand = source.shellCommands.removeCommand
    shell.renameCommand = source.shellCommands.renameCommand
    shell.pwdCommand = source.shellCommands.pwdCommand
    shell.realpathCommand = source.shellCommands.realpathCommand
    shell.symlinkCommand = source.shellCommands.symlinkCommand
    shell.readlinkCommand = source.shellCommands.readlinkCommand
    shell.clearAliases = source.shellCommands.clearAliases
    shell.clearNationalVars = source.shellCommands.clearNationalVars
    shell.tryFullTime = source.shellCommands.tryFullTime
    shell.ignoreLsWarnings = source.shellCommands.ignoreLsWarnings
    shell.allowScpFallback = source.shellCommands.allowScpFallback
    copy.shellCommands = shell
    return copy
}
