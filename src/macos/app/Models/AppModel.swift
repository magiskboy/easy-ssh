// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import Combine
import Foundation
import SwiftUI

enum SidebarMode: String, CaseIterable, Identifiable, Hashable {
    case sessions
    case files
    case tunnels
    case explorers

    var id: String { rawValue }

    var title: String {
        switch self {
        case .sessions: return "Sessions"
        case .files: return "Files"
        case .tunnels: return "Tunnels"
        case .explorers: return "Explorers"
        }
    }

    var systemImage: String {
        switch self {
        case .sessions: return "terminal"
        case .files: return "folder"
        case .tunnels: return "network"
        case .explorers: return "server.rack"
        }
    }

    var isImplemented: Bool { self == .sessions }

    var comingSoonMessage: String {
        switch self {
        case .sessions:
            return "Connect to a host to open a terminal session."
        case .files:
            return "File Explorer will plug into the same session bridge (Phase 5)."
        case .tunnels:
            return "Tunnel management comes in Phase 10."
        case .explorers:
            return "Remote explorers (process/container/service/system) come in Phase 6."
        }
    }
}

struct PasswordPromptRequest: Identifiable {
    let id = UUID()
    let connection: ESSConnectionInfo
    /// Pre-loaded gateway secret (if any) so the target password prompt does not drop it.
    var gatewaySecret: String? = nil
}

@MainActor
final class AppModel: ObservableObject {
    let status = StatusBannerModel()
    let library = ConnectionLibrary()

    @Published var sidebarMode: SidebarMode = .sessions
    @Published var sessions: [SessionViewModel] = []
    @Published var selectedSessionId: UUID?
    @Published var showConnectSheet: Bool = false
    @Published var showConnectionManager: Bool = false
    @Published var showAbout: Bool = false
    @Published var connectDraft = ConnectionDraft()
    @Published var passwordPrompt: PasswordPromptRequest?
    @Published var passwordPromptValue: String = ""

    private var cancellables = Set<AnyCancellable>()

    init() {
        status.objectWillChange
            .sink { [weak self] _ in
                self?.objectWillChange.send()
            }
            .store(in: &cancellables)

        library.objectWillChange
            .sink { [weak self] _ in
                self?.objectWillChange.send()
            }
            .store(in: &cancellables)
    }

    var selectedSession: SessionViewModel? {
        sessions.first { $0.id == selectedSessionId }
    }

    var recentConnections: [ESSConnectionInfo] {
        library.recentConnections(limit: 8)
    }

    var sidebarModeBinding: Binding<SidebarMode> {
        Binding(
            get: { self.sidebarMode },
            set: { newValue in
                guard newValue.isImplemented else { return }
                self.sidebarMode = newValue
            }
        )
    }

    func openConnectSheet() {
        connectDraft = ConnectionDraft()
        showConnectSheet = true
    }

    func openConnectionManager() {
        library.reload()
        showConnectionManager = true
    }

    func connect(with draft: ConnectionDraft) {
        let form = ConnectionFormState.from(draft: draft)
        if draft.saveConnection {
            let info = form.makeConnectionInfo()
            guard library.add(info) else {
                status.notify(
                    title: "Save Connection",
                    message: "Could not save the connection to disk.",
                    level: .error
                )
                return
            }
            ConnectionSecretHelper.persistSecrets(
                for: info,
                previousAuthType: nil,
                isEdit: false,
                password: draft.usePrivateKey ? nil : draft.password,
                passphrase: draft.usePrivateKey ? draft.passphrase : nil
            )
            openSession(connection: info, credentials: form.makeCredentials())
        } else {
            openSession(connection: form.makeConnectionInfo(), credentials: form.makeCredentials())
        }
        showConnectSheet = false
    }

    func connect(withId id: UUID) {
        guard let info = library.connection(id: id) else {
            status.notify(
                title: "Connect",
                message: "Connection not found.",
                level: .error
            )
            return
        }
        connect(with: info, inlineCredentials: nil)
    }

    func connect(with info: ESSConnectionInfo, inlineCredentials: ESSSessionCredentials?) {
        if let inline = inlineCredentials,
           let secret = inline.targetSecret,
           !secret.isEmpty
        {
            if ConnectionSecretHelper.needsGatewaySecret(info),
               inline.gatewaySecret == nil || inline.gatewaySecret?.isEmpty == true
            {
                ConnectionSecretHelper.loadGatewaySecret(for: info) { [weak self] gateway in
                    Task { @MainActor in
                        guard let self else { return }
                        if Self.missingRequiredGatewaySecret(info, gateway: gateway) {
                            self.status.notify(
                                title: "Gateway Credentials",
                                message:
                                    "ProxyJump gateway credentials are missing. "
                                    + "Edit the connection and set the gateway password or passphrase.",
                                level: .error
                            )
                            return
                        }
                        inline.gatewaySecret = gateway
                        self.openSession(connection: info, credentials: inline)
                    }
                }
                return
            }
            openSession(connection: info, credentials: inline)
            return
        }

        ConnectionSecretHelper.loadCredentials(for: info) { [weak self] target, gateway in
            Task { @MainActor in
                guard let self else { return }

                if Self.missingRequiredGatewaySecret(info, gateway: gateway) {
                    self.status.notify(
                        title: "Gateway Credentials",
                        message:
                            "ProxyJump gateway credentials are missing. "
                            + "Edit the connection and set the gateway password or passphrase.",
                        level: .error
                    )
                    return
                }

                if info.authType == .password {
                    if let target, !target.isEmpty {
                        let creds = ESSSessionCredentials()
                        creds.targetSecret = target
                        creds.gatewaySecret = gateway
                        self.openSession(connection: info, credentials: creds)
                    } else {
                        self.passwordPromptValue = ""
                        self.passwordPrompt = PasswordPromptRequest(
                            connection: info,
                            gatewaySecret: gateway
                        )
                    }
                } else {
                    let creds = ESSSessionCredentials()
                    creds.targetSecret = target
                    creds.gatewaySecret = gateway
                    self.openSession(connection: info, credentials: creds)
                }
            }
        }
    }

    func submitPasswordPrompt() {
        guard let request = passwordPrompt else { return }
        let creds = ESSSessionCredentials()
        creds.targetSecret = passwordPromptValue
        creds.gatewaySecret = request.gatewaySecret
        passwordPrompt = nil
        passwordPromptValue = ""
        openSession(connection: request.connection, credentials: creds)
    }

    func cancelPasswordPrompt() {
        passwordPrompt = nil
        passwordPromptValue = ""
    }

    func closeSession(_ id: UUID) {
        let name = sessions.first { $0.id == id }?.title
        if let idx = sessions.firstIndex(where: { $0.id == id }) {
            sessions[idx].disconnect()
            sessions.remove(at: idx)
        }
        if selectedSessionId == id {
            selectedSessionId = sessions.last?.id
        }
        if let name {
            status.post("Closed session: \(name)", level: .warning)
        }
    }

    func closeSelectedSession() {
        guard let id = selectedSessionId else { return }
        closeSession(id)
    }

    func disconnectSelectedSession() {
        guard let session = selectedSession else { return }
        session.disconnect()
        status.post("Disconnected: \(session.title)", level: .warning)
    }

    func reconnectSelectedSession() {
        guard let session = selectedSession else { return }
        status.post("Connecting: \(session.title)…", level: .status)
        session.reconnect()
    }

    func selectNextSession() {
        guard !sessions.isEmpty else { return }
        guard let current = selectedSessionId,
              let idx = sessions.firstIndex(where: { $0.id == current })
        else {
            selectedSessionId = sessions.first?.id
            return
        }
        let next = sessions.index(after: idx)
        selectedSessionId = sessions[next == sessions.endIndex ? sessions.startIndex : next].id
    }

    func selectPreviousSession() {
        guard !sessions.isEmpty else { return }
        guard let current = selectedSessionId,
              let idx = sessions.firstIndex(where: { $0.id == current })
        else {
            selectedSessionId = sessions.last?.id
            return
        }
        let prev = idx == sessions.startIndex ? sessions.index(before: sessions.endIndex) : sessions.index(before: idx)
        selectedSessionId = sessions[prev].id
    }

    func pasteClipboardIntoActiveSession() {
        selectedSession?.pasteClipboard()
    }

    func openLogFile() {
        let path = EasySshRuntime.logFilePath()
        let url = URL(fileURLWithPath: path)
        if NSWorkspace.shared.open(url) {
            status.post("Opened log file", level: .status)
        } else {
            status.notify(
                title: "Open Log",
                message: "Cannot open log file with the system application: \(path)",
                level: .error
            )
        }
    }

    // MARK: - Private

    /// Gateway password auth requires a non-empty secret; private-key passphrase may be empty.
    private static func missingRequiredGatewaySecret(
        _ info: ESSConnectionInfo,
        gateway: String?
    ) -> Bool {
        guard ConnectionSecretHelper.needsGatewaySecret(info) else { return false }
        let hop = info.jumpHops[0]
        guard hop.authType == .password else { return false }
        return gateway == nil || gateway?.isEmpty == true
    }

    private func openSession(connection: ESSConnectionInfo, credentials: ESSSessionCredentials?) {
        let session = SessionViewModel(connection: connection, credentials: credentials)
        wireSession(session)
        sessions.append(session)
        selectedSessionId = session.id
        showConnectSheet = false
        showConnectionManager = false
        sidebarMode = .sessions
        status.post("Connecting: \(session.title)…", level: .status)
        session.connect()
    }

    private func wireSession(_ session: SessionViewModel) {
        session.onStatus = { [weak self] message, level in
            self?.status.post(message, level: level)
        }
        session.onConnectedOnce = { connectionId in
            ESSAppSettings.shared().recordRecentConnection(connectionId)
        }
    }
}
