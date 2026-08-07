// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

enum TunnelTypeModel: String, CaseIterable, Identifiable {
    case local
    case remote
    case dynamic

    var id: String { rawValue }

    var title: String {
        switch self {
        case .local: return "Local"
        case .remote: return "Remote"
        case .dynamic: return "Dynamic"
        }
    }
}

enum TunnelEndpointKindModel: String, CaseIterable, Identifiable {
    case tcp
    case unix

    var id: String { rawValue }
}

enum TunnelSocksAuthModeModel: String, CaseIterable, Identifiable {
    case none
    case usernamePassword

    var id: String { rawValue }

    var title: String {
        switch self {
        case .none: return "None"
        case .usernamePassword: return "Username + Password"
        }
    }
}

struct TunnelDefinition: Identifiable, Hashable {
    var id: UUID = UUID()
    var connectionId: UUID
    var name: String = ""
    var type: TunnelTypeModel = .local
    var enabled = true

    var localKind: TunnelEndpointKindModel = .tcp
    var localHost = "127.0.0.1"
    var localPort = 0
    var localSocketPath = ""

    var remoteKind: TunnelEndpointKindModel = .tcp
    var remoteHost = "127.0.0.1"
    var remotePort = 0
    var remoteSocketPath = ""

    var socksAuth: TunnelSocksAuthModeModel = .none
    var socksUsername = ""

    var localAddress: String {
        if localKind == .unix { return localSocketPath }
        return "\(localHost):\(localPort)"
    }

    var remoteAddress: String {
        if type == .dynamic { return "SOCKS5" }
        if remoteKind == .unix { return remoteSocketPath }
        return "\(remoteHost):\(remotePort)"
    }

    var typeTitle: String { type.title }

    init(connectionId: UUID) {
        self.connectionId = connectionId
    }

    init?(dict: [AnyHashable: Any]) {
        guard let connectionId = dict["connectionId"] as? UUID else { return nil }
        self.connectionId = connectionId
        self.id = (dict["id"] as? UUID) ?? UUID()
        self.name = dict["name"] as? String ?? ""
        self.type = TunnelTypeModel(rawValue: dict["type"] as? String ?? "") ?? .local
        self.enabled = (dict["enabled"] as? NSNumber)?.boolValue ?? true
        self.localKind = TunnelEndpointKindModel(rawValue: dict["localKind"] as? String ?? "") ?? .tcp
        self.localHost = dict["localHost"] as? String ?? "127.0.0.1"
        self.localPort = (dict["localPort"] as? NSNumber)?.intValue ?? 0
        self.localSocketPath = dict["localSocketPath"] as? String ?? ""
        self.remoteKind = TunnelEndpointKindModel(rawValue: dict["remoteKind"] as? String ?? "") ?? .tcp
        self.remoteHost = dict["remoteHost"] as? String ?? "127.0.0.1"
        self.remotePort = (dict["remotePort"] as? NSNumber)?.intValue ?? 0
        self.remoteSocketPath = dict["remoteSocketPath"] as? String ?? ""
        self.socksAuth = TunnelSocksAuthModeModel(rawValue: dict["socksAuth"] as? String ?? "")
            ?? .none
        self.socksUsername = dict["socksUsername"] as? String ?? ""
    }

    var bridgeDictionary: [String: Any] {
        [
            "id": id,
            "connectionId": connectionId,
            "name": name,
            "type": type.rawValue,
            "enabled": enabled,
            "localKind": localKind.rawValue,
            "localHost": localHost,
            "localPort": localPort,
            "localSocketPath": localSocketPath,
            "remoteKind": remoteKind.rawValue,
            "remoteHost": remoteHost,
            "remotePort": remotePort,
            "remoteSocketPath": remoteSocketPath,
            "socksAuth": socksAuth.rawValue,
            "socksUsername": socksUsername,
        ]
    }

    func bridgeDictionary(socksPassword: String?) -> [String: Any] {
        var dict = bridgeDictionary
        if let socksPassword {
            dict["socksPassword"] = socksPassword
        }
        return dict
    }
}

struct TunnelRow: Identifiable, Hashable {
    var definition: TunnelDefinition
    var status = "Off"
    var statusDetail = ""

    var id: UUID { definition.id }
    var name: String { definition.name }
    var typeTitle: String { definition.typeTitle }
    var localAddress: String { definition.localAddress }
    var remoteAddress: String { definition.remoteAddress }
    var isRunning: Bool { status == "Listening" || status == "Starting" }
}

struct TunnelEditorContext: Identifiable {
    enum Mode {
        case create
        case edit
    }

    let id = UUID()
    let mode: Mode
    var definition: TunnelDefinition
    var socksPassword: String
}

@MainActor
final class SessionTunnelsModel: ObservableObject {
    @Published private(set) var rows: [TunnelRow] = []
    @Published var selectedTunnelId: UUID?
    @Published var editor: TunnelEditorContext?
    @Published var pendingDelete: TunnelDefinition?

    private weak var session: SessionViewModel?
    private var controller: ESSSessionController?

    var selectedRow: TunnelRow? {
        guard let selectedTunnelId else { return nil }
        return rows.first { $0.id == selectedTunnelId }
    }

    var canEditSelection: Bool {
        guard let selectedRow else { return false }
        return !selectedRow.isRunning
    }

    func attach(to session: SessionViewModel) {
        if self.session !== session {
            self.session = session
            controller = session.sessionController
            wireController()
            reloadFromStore()
        } else if rows.isEmpty {
            reloadFromStore()
        }
    }

    func onSessionConnected() {
        reloadFromStore()
        startEnabledTunnels()
    }

    func onSessionDisconnected() {
        clearRuntimeStatuses()
    }

    func reloadFromStore() {
        guard let session else {
            rows = []
            return
        }
        rows = ESSTunnelStore.load(forConnectionId: session.connection.connectionId)
            .compactMap { TunnelDefinition(dict: $0) }
            .map { TunnelRow(definition: $0) }
        if let selectedTunnelId, !rows.contains(where: { $0.id == selectedTunnelId }) {
            self.selectedTunnelId = nil
        }
    }

    func beginCreate() {
        guard let session else { return }
        editor = TunnelEditorContext(
            mode: .create,
            definition: TunnelDefinition(connectionId: session.connection.connectionId as UUID),
            socksPassword: ""
        )
    }

    func beginEditSelection() {
        guard let row = selectedRow else { return }
        beginEdit(row.definition)
    }

    func beginEdit(_ definition: TunnelDefinition) {
        guard !runtimeStatus(for: definition.id).isRunning else {
            session?.onStatus?("Disable the tunnel before editing.", .warning)
            return
        }
        editor = TunnelEditorContext(mode: .edit, definition: definition, socksPassword: "")
        if definition.type == .dynamic, definition.socksAuth == .usernamePassword {
            ESSSecretStore.shared().readSecret(
                for: definition.id,
                kind: .tunnelSocksPassword
            ) { [weak self] value, ok, _ in
                Task { @MainActor in
                    guard let self, self.editor?.definition.id == definition.id else { return }
                    if ok {
                        self.editor?.socksPassword = value ?? ""
                    }
                }
            }
        }
    }

    func saveEditor(definition: TunnelDefinition, socksPassword: String) -> String? {
        guard let session, let controller else { return "Tunnel session is unavailable." }
        let message = controller.validationError(forTunnel: definition.bridgeDictionary)
        if !message.isEmpty { return message }

        let isEdit = rows.contains(where: { $0.id == definition.id })
        storeSocksSecret(for: definition, password: socksPassword)
        upsert(definition)
        persistAll()
        editor = nil

        if definition.enabled, session.state == .connected {
            startTunnel(definition, socksPassword: socksPassword)
        } else if !definition.enabled {
            updateStatus(for: definition.id, status: "Off", detail: "")
        }

        let verb = isEdit ? "updated" : "added"
        session.onStatus?("Tunnel \(verb): \(definition.name)", isEdit ? .success : .success)
        return nil
    }

    func requestDeleteSelection() {
        pendingDelete = selectedRow?.definition
    }

    func deletePending() {
        guard let pendingDelete else { return }
        delete(pendingDelete)
        self.pendingDelete = nil
    }

    func delete(_ definition: TunnelDefinition) {
        if session?.state == .connected {
            controller?.stopTunnel(definition.id)
        }
        ESSSecretStore.shared().deleteSecret(for: definition.id, kind: .tunnelSocksPassword) { _, _ in }
        rows.removeAll { $0.id == definition.id }
        if selectedTunnelId == definition.id {
            selectedTunnelId = nil
        }
        persistAll()
        session?.onStatus?("Tunnel deleted: \(definition.name)", .warning)
    }

    func toggleSelectionEnabled() {
        guard let row = selectedRow else { return }
        toggle(definition: row.definition)
    }

    func toggle(definition: TunnelDefinition) {
        var next = definition
        let running = runtimeStatus(for: definition.id).isRunning
        next.enabled = !running
        upsert(next)
        persistAll()

        if session?.state == .connected {
            if running {
                controller?.stopTunnel(definition.id)
            } else {
                startTunnel(next, socksPassword: nil)
            }
        } else {
            updateStatus(for: next.id, status: "Off", detail: "")
        }

        let message = next.enabled
            ? (session?.state == .connected
                ? "Tunnel enabled: \(next.name)"
                : "Tunnel will enable on next connect: \(next.name)")
            : "Tunnel disabled: \(next.name)"
        session?.onStatus?(message, .status)
    }

    func stopAll() {
        controller?.stopAllTunnels()
    }

    private func wireController() {
        controller?.onTunnelStatusChanged = { [weak self] tunnelId, status, detail in
            Task { @MainActor in
                self?.updateStatus(for: tunnelId as UUID, status: status, detail: detail)
            }
        }
        controller?.onTunnelError = { [weak self] tunnelId, message in
            Task { @MainActor in
                guard let self else { return }
                self.updateStatus(for: tunnelId as UUID, status: "Error", detail: message)
                self.session?.onStatus?("Tunnel: \(message)", .error)
            }
        }
    }

    private func startEnabledTunnels() {
        for row in rows where row.definition.enabled {
            startTunnel(row.definition, socksPassword: nil)
        }
    }

    private func startTunnel(_ definition: TunnelDefinition, socksPassword: String?) {
        guard let session, session.state == .connected, let controller else { return }
        if definition.type == .dynamic, definition.socksAuth == .usernamePassword {
            let id = definition.id
            ESSSecretStore.shared().readSecret(for: id, kind: .tunnelSocksPassword) { [weak self] value, ok, _ in
                Task { @MainActor in
                    guard let self,
                          let session = self.session,
                          session.state == .connected
                    else { return }
                    let password = socksPassword ?? (ok ? value : nil)
                    controller.startTunnel(definition.bridgeDictionary(socksPassword: password))
                }
            }
            return
        }
        controller.startTunnel(definition.bridgeDictionary(socksPassword: socksPassword))
    }

    private func storeSocksSecret(for definition: TunnelDefinition, password: String) {
        let store = ESSSecretStore.shared()
        if definition.type == .dynamic,
           definition.socksAuth == .usernamePassword,
           !password.isEmpty
        {
            store.storeSecret(for: definition.id, kind: .tunnelSocksPassword, value: password) { _, _ in }
        } else {
            store.deleteSecret(for: definition.id, kind: .tunnelSocksPassword) { _, _ in }
        }
    }

    private func persistAll() {
        guard let connectionId = session?.connection.connectionId as UUID? else { return }
        let kept = ESSTunnelStore.loadAll()
            .compactMap { TunnelDefinition(dict: $0) }
            .filter { $0.connectionId != connectionId }
        ESSTunnelStore.saveAll(kept.map(\.bridgeDictionary) + rows.map(\.definition.bridgeDictionary))
    }

    private func upsert(_ definition: TunnelDefinition) {
        if let idx = rows.firstIndex(where: { $0.id == definition.id }) {
            rows[idx].definition = definition
        } else {
            rows.append(TunnelRow(definition: definition))
        }
        selectedTunnelId = definition.id
    }

    private func runtimeStatus(for id: UUID) -> TunnelRow {
        rows.first(where: { $0.id == id }) ?? TunnelRow(definition: TunnelDefinition(connectionId: UUID()))
    }

    private func updateStatus(for id: UUID, status: String, detail: String) {
        guard let idx = rows.firstIndex(where: { $0.id == id }) else { return }
        rows[idx].status = status
        rows[idx].statusDetail = detail
    }

    private func clearRuntimeStatuses() {
        for idx in rows.indices {
            rows[idx].status = "Off"
            rows[idx].statusDetail = ""
        }
    }
}
