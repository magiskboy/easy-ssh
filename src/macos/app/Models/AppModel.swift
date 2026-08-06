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

    var isImplemented: Bool { self == .sessions || self == .files || self == .explorers }

    var comingSoonMessage: String {
        switch self {
        case .sessions:
            return "Connect to a host to open a terminal session."
        case .files:
            return "Connect to a session to browse remote files."
        case .tunnels:
            return "Tunnel management comes in Phase 10."
        case .explorers:
            return "Connect to a session to browse processes, containers, services, and system info."
        }
    }
}

struct PasswordPromptRequest: Identifiable {
    let id = UUID()
    let connection: ESSConnectionInfo
    /// Pre-loaded gateway secret (if any) so the target password prompt does not drop it.
    var gatewaySecret: String? = nil
}

enum AppModal: Identifiable, Equatable {
    case connect
    case connectionManager
    case passwordPrompt
    case hostKeyPrompt
    case about

    var id: String {
        switch self {
        case .connect: return "connect"
        case .connectionManager: return "connectionManager"
        case .passwordPrompt: return "passwordPrompt"
        case .hostKeyPrompt: return "hostKeyPrompt"
        case .about: return "about"
        }
    }
}

@MainActor
final class AppModel: ObservableObject {
    let status = StatusBannerModel()
    let library = ConnectionLibrary()

    @Published var sidebarMode: SidebarMode = .sessions
    @Published var sessions: [SessionViewModel] = []
    @Published var selectedSessionId: UUID?
    @Published var activeModal: AppModal?
    @Published var connectDraft = ConnectionDraft()
    @Published var passwordPrompt: PasswordPromptRequest?
    @Published var passwordPromptValue: String = ""
    /// Session waiting on the shared host-key sheet (presented via `activeModal`).
    @Published var hostKeySessionId: UUID?
    /// Bumped when persisted settings are applied so views re-read ESSAppSettings.
    @Published var settingsEpoch = 0
    /// Select this tab when the Settings window opens.
    @Published var pendingSettingsTab: SettingsTab?
    /// Cancels stale keychain / timeout completions when a newer connect starts.
    private var connectAttemptID = UUID()

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

        ESSAppSettings.shared().onSettingsChanged = { [weak self] in
            Task { @MainActor in
                guard let self else { return }
                self.settingsEpoch &+= 1
                for session in self.sessions {
                    session.refreshAppearance()
                    session.files?.applySettingsChanged()
                }
            }
        }
    }

    func openSettings(tab: SettingsTab = .general) {
        pendingSettingsTab = tab
        NSApp.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
    }

    func shortcutPortable(for actionId: String) -> String? {
        _ = settingsEpoch
        return ESSAppSettings.shared().shortcut(forActionId: actionId)
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
        activeModal = .connect
    }

    func openConnectionManager() {
        library.reload()
        activeModal = .connectionManager
    }

    /// Close Connection Manager, then connect on the next turn so a password sheet can present
    /// on the same `.sheet(item:)` host (macOS drops stacked sheets).
    func openSessionFromManager(connectionId: UUID) {
        guard let info = library.connection(id: connectionId) else {
            status.notify(
                title: "Connect",
                message: "Connection not found.",
                level: .error
            )
            if activeModal == .connectionManager {
                activeModal = nil
            }
            return
        }
        openSessionFromManager(connection: info)
    }

    func openSessionFromManager(connection: ESSConnectionInfo) {
        if activeModal == .connectionManager {
            activeModal = nil
        }
        let label = connection.name.trimmingCharacters(in: .whitespacesAndNewlines)
        let title = label.isEmpty
            ? (connection.displayText.isEmpty
                ? "\(connection.username)@\(connection.host)"
                : connection.displayText)
            : label
        status.post("Opening: \(title)…", level: .status)

        // Password auth needs a tick so the manager sheet can finish dismissing before the
        // password sheet presents on the same host. Private-key sessions need no modal.
        if connection.authType == .password {
            DispatchQueue.main.async { [weak self] in
                self?.connect(with: connection, inlineCredentials: nil)
            }
        } else {
            connect(with: connection, inlineCredentials: nil)
        }
    }

    private func dismissModals() {
        activeModal = nil
        passwordPrompt = nil
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
        dismissModals()
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

        // Private key / agent without a custom gateway: do not wait on Qt keychain.
        // Unencrypted keys and ssh-agent work with an empty passphrase; encrypted keys
        // can still be saved via New Connection with an inline passphrase.
        if info.authType != .password, !ConnectionSecretHelper.needsGatewaySecret(info) {
            let creds = ESSSessionCredentials()
            openSession(connection: info, credentials: creds)
            return
        }

        let attempt = UUID()
        connectAttemptID = attempt

        ConnectionSecretHelper.loadCredentials(for: info) { [weak self] target, gateway in
            Task { @MainActor in
                guard let self, self.connectAttemptID == attempt else { return }
                self.connectAttemptID = UUID()
                self.finishConnectAfterSecrets(info: info, target: target, gateway: gateway)
            }
        }

        // Qt keychain jobs can stall when QCoreApplication has no dedicated exec loop.
        // Unblock the UI so password prompt / session still appears.
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.45) { [weak self] in
            guard let self, self.connectAttemptID == attempt else { return }
            self.connectAttemptID = UUID()
            self.finishConnectAfterSecrets(info: info, target: nil, gateway: nil)
        }
    }

    private func finishConnectAfterSecrets(
        info: ESSConnectionInfo,
        target: String?,
        gateway: String?
    ) {
        if Self.missingRequiredGatewaySecret(info, gateway: gateway) {
            status.notify(
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
                openSession(connection: info, credentials: creds)
            } else {
                presentPasswordPrompt(connection: info, gatewaySecret: gateway)
            }
        } else {
            let creds = ESSSessionCredentials()
            creds.targetSecret = target
            creds.gatewaySecret = gateway
            openSession(connection: info, credentials: creds)
        }
    }

    private func presentPasswordPrompt(connection: ESSConnectionInfo, gatewaySecret: String?) {
        passwordPromptValue = ""
        passwordPrompt = PasswordPromptRequest(
            connection: connection,
            gatewaySecret: gatewaySecret
        )
        // Defer so a just-dismissed Connection Manager sheet can finish tearing down.
        DispatchQueue.main.async { [weak self] in
            self?.activeModal = .passwordPrompt
        }
    }

    func submitPasswordPrompt() {
        guard let request = passwordPrompt else { return }
        let creds = ESSSessionCredentials()
        creds.targetSecret = passwordPromptValue
        creds.gatewaySecret = request.gatewaySecret
        passwordPrompt = nil
        passwordPromptValue = ""
        activeModal = nil
        openSession(connection: request.connection, credentials: creds)
    }

    func cancelPasswordPrompt() {
        passwordPrompt = nil
        passwordPromptValue = ""
        if activeModal == .passwordPrompt {
            activeModal = nil
        }
    }

    func closeSession(_ id: UUID) {
        if hostKeySessionId == id {
            hostKeySessionId = nil
            if activeModal == .hostKeyPrompt {
                activeModal = nil
            }
        }
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

    func copySelectionFromActiveSession() {
        selectedSession?.copySelection()
    }

    func openShellInSelectedSession() {
        selectedSession?.openShell()
    }

    func closeShellInSelectedSession() {
        selectedSession?.closeFocusedShell()
    }

    func renameShellInSelectedSession() {
        selectedSession?.beginRenameFocusedShell()
    }

    func focusShellInSelectedSession(_ shellId: UUID) {
        selectedSession?.focusShell(shellId)
    }

    func clearTerminalInSelectedSession() {
        selectedSession?.clearFocusedTerminal()
    }

    func toggleFindInSelectedSession() {
        selectedSession?.toggleFindBar()
    }

    func saveLogInSelectedSession() {
        selectedSession?.saveLogForFocusedShell()
    }

    func saveScreenshotInSelectedSession() {
        selectedSession?.saveScreenshotForFocusedShell()
    }

    var canUseTerminalActions: Bool {
        selectedSession?.state == .connected && selectedSession?.focusedShell != nil
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
        dismissModals()
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
        session.onHostKeyPromptUI = { [weak self, weak session] _ in
            guard let self, let session else { return }
            self.hostKeySessionId = session.id
            // Present on the shared sheet host — SessionPane cannot stack another .sheet on macOS.
            DispatchQueue.main.async {
                self.activeModal = .hostKeyPrompt
            }
        }
    }

    func respondHostKey(accept: Bool) {
        let session = hostKeySessionId.flatMap { id in sessions.first { $0.id == id } }
            ?? selectedSession
        hostKeySessionId = nil
        if activeModal == .hostKeyPrompt {
            activeModal = nil
        }
        session?.respondHostKey(accept: accept)
    }

    var activeHostKeyPrompt: HostKeyPromptData? {
        let session = hostKeySessionId.flatMap { id in sessions.first { $0.id == id } }
            ?? selectedSession
        return session?.hostKeyPrompt
    }
}
