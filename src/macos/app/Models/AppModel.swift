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

    var isImplemented: Bool {
        self == .sessions || self == .files || self == .tunnels || self == .explorers
    }

    var comingSoonMessage: String {
        switch self {
        case .sessions:
            return "Connect to a host to open a terminal session."
        case .files:
            return "Connect to a session to browse remote files."
        case .tunnels:
            return "Connect to a session to manage local, remote, and dynamic tunnels."
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
    case explorer

    var id: String {
        switch self {
        case .connect: return "connect"
        case .connectionManager: return "connectionManager"
        case .passwordPrompt: return "passwordPrompt"
        case .hostKeyPrompt: return "hostKeyPrompt"
        case .about: return "about"
        case .explorer: return "explorer"
        }
    }
}

@MainActor
final class AppModel: ObservableObject {
    let status = StatusBannerModel()
    let library = ConnectionLibrary()

    @Published var sidebarMode: SidebarMode = .sessions
    @Published var sessions: [SessionViewModel] = []
    @Published var selectedConnectionId: UUID?
    @Published var selectedSessionId: UUID? {
        didSet {
            syncSelectedConnectionFromSession()
        }
    }
    @Published var activeModal: AppModal?
    @Published var explorerDialogKind: ExplorerKind?
    @Published var connectDraft = ConnectionDraft()
    @Published var passwordPrompt: PasswordPromptRequest?
    @Published var passwordPromptValue: String = ""
    /// Session waiting on the shared host-key sheet (presented via `activeModal`).
    @Published var hostKeySessionId: UUID?
    /// Bumped when persisted settings are applied so views re-read ESSAppSettings.
    @Published var settingsEpoch = 0
    /// Select this tab when the Settings window opens.
    @Published var pendingSettingsTab: SettingsTab?
    /// Pre-filled create query when opening Connection Manager from palette.
    @Published var connectionManagerCreateQuery: String?
    /// Command palette presentation (separate from `activeModal` to avoid stacking issues).
    @Published var paletteMode: PaletteMode?

    let workspace: WorkspaceCoordinator
    var tray: TrayController?

    /// Cancels stale keychain / timeout completions when a newer connect starts.
    private var connectAttemptID = UUID()
    private var pendingWorkspaceEntriesByConnectionId: [UUID: ESSWorkspaceSessionEntry] = [:]
    private var pendingWorkspaceRestoreReadyByConnectionId: [UUID: (SessionViewModel?) -> Void] = [:]

    private var cancellables = Set<AnyCancellable>()

    init() {
        workspace = WorkspaceCoordinator()
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

        workspace.appModel = self
        tray = TrayController(appModel: self)
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

    var selectedConnection: ESSConnectionInfo? {
        guard let selectedConnectionId else { return nil }
        return library.connection(id: selectedConnectionId)
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

    func selectConnection(_ connectionId: UUID) {
        selectedConnectionId = connectionId
        openOrSelectSession(for: connectionId)
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

        if selectExistingSession(for: connection.connectionId as UUID) {
            return
        }

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
        if selectExistingSession(for: id) {
            return
        }
        guard let info = library.connection(id: id) else {
            status.notify(
                title: "Connect",
                message: "Connection not found.",
                level: .error
            )
            resolvePendingWorkspaceRestore(connectionId: id, session: nil)
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
            resolvePendingWorkspaceRestore(connectionId: info.connectionId as UUID, session: nil)
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
        let request = passwordPrompt
        passwordPrompt = nil
        passwordPromptValue = ""
        if activeModal == .passwordPrompt {
            activeModal = nil
        }
        if let request {
            resolvePendingWorkspaceRestore(
                connectionId: request.connection.connectionId as UUID,
                session: nil
            )
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
        workspace.scheduleSave()
        tray?.refresh()
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
        if let entry = pendingWorkspaceEntriesByConnectionId.removeValue(forKey: connection.connectionId as UUID) {
            session.beginWorkspaceRestore(entry)
        }
        wireSession(session)
        resolvePendingWorkspaceRestore(connectionId: connection.connectionId as UUID, session: session)
        sessions.append(session)
        selectedSessionId = session.id
        dismissModals()
        sidebarMode = .sessions
        status.post("Connecting: \(session.title)…", level: .status)
        session.connect()
        workspace.scheduleSave()
    }

    private func wireSession(_ session: SessionViewModel) {
        session.onStatus = { [weak self] message, level in
            self?.status.post(message, level: level)
            self?.tray?.refresh()
            if level == .error || level == .warning {
                let title = message.hasPrefix("Tunnel:") ? "Tunnel Error" : "Easy SSH"
                self?.tray?.maybeNotify(title: title, message: message)
            }
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

    // MARK: - Command palette

    func openCommandPalette() {
        paletteMode = .actions
    }

    func openQuickConnect() {
        paletteMode = .connections
    }

    func openGoToShell() {
        paletteMode = .shells
    }

    func dismissCommandPalette() {
        paletteMode = nil
    }

    func paletteRows(mode: PaletteMode, query: String) -> [PaletteRow] {
        switch mode {
        case .actions:
            return buildActionRows(query: query)
        case .connections:
            return buildConnectionRows(query: query)
        case .shells:
            return buildShellRows(query: query)
        }
    }

    func activatePaletteRow(_ row: PaletteRow, filter: String) {
        switch row {
        case let .action(item, _):
            performPaletteAction(id: item.actionId)
        case let .connection(item, _):
            dismissCommandPalette()
            connect(withId: item.connectionId)
        case .createConnection:
            dismissCommandPalette()
            createConnection(fromQuery: filter)
        case let .shell(item, _):
            dismissCommandPalette()
            focusShell(connectionId: item.connectionId, shellId: item.shellId)
        case .hint:
            break
        }
    }

    func createConnection(fromQuery query: String) {
        connectionManagerCreateQuery = query
        openConnectionManager()
    }

    func focusShell(connectionId: UUID, shellId: UUID) {
        guard let session = session(forConnectionId: connectionId) else { return }
        selectedConnectionId = connectionId
        selectedSessionId = session.id
        sidebarMode = .sessions
        session.focusShell(shellId)
    }

    func performPaletteAction(id actionId: String) {
        dismissCommandPalette()
        switch actionId {
        case "general.newConnection":
            openConnectSheet()
        case "general.connectionManager":
            openConnectionManager()
        case "general.settings":
            openSettings(tab: .general)
        case "general.shortcuts":
            openSettings(tab: .shortcuts)
        case "general.about":
            activeModal = .about
        case "session.newSession":
            openShellInSelectedSession()
        case "shell.close":
            closeShellInSelectedSession()
        case "session.nextTab":
            selectNextSession()
        case "session.previousTab":
            selectPreviousSession()
        case "session.processExplorer":
            openExplorer(.process)
        case "session.containerExplorer":
            openExplorer(.container)
        case "session.serviceExplorer":
            openExplorer(.service)
        case "session.systemInfo":
            openExplorer(.systemInfo)
        case "terminal.copy":
            copySelectionFromActiveSession()
        case "terminal.paste":
            pasteClipboardIntoActiveSession()
        case "terminal.clearScreen":
            clearTerminalInSelectedSession()
        case "terminal.search":
            toggleFindInSelectedSession()
        case "terminal.saveLog":
            saveLogInSelectedSession()
        case "terminal.saveScreenshot":
            saveScreenshotInSelectedSession()
        default:
            status.post("Action not available: \(actionId)", level: .warning)
        }
    }

    func confirmQuitWithActiveSessions() -> Bool {
        let active = sessions.filter { $0.state == .connecting || $0.state == .connected }.count
        guard active > 0 else { return true }

        if !(tray?.isAvailable == true) || NSApp.windows.contains(where: { $0.isVisible && $0.canBecomeMain }) {
            // show window for alert if hidden
        } else {
            tray?.showWindow()
        }

        let alert = NSAlert()
        alert.messageText = "Quit Easy SSH?"
        alert.informativeText = "\(active) session(s) are still active. Quit anyway?"
        alert.alertStyle = .warning
        alert.addButton(withTitle: "Quit")
        alert.addButton(withTitle: "Cancel")
        return alert.runModal() == .alertFirstButtonReturn
    }

    func session(forConnectionId connectionId: UUID) -> SessionViewModel? {
        sessions.first { ($0.connection.connectionId as UUID) == connectionId }
    }

    func openOrSelectSession(for connectionId: UUID) {
        if selectExistingSession(for: connectionId) {
            return
        }
        connect(withId: connectionId)
    }

    func presentExplorer(_ kind: ExplorerKind) {
        openExplorer(kind)
    }

    func openSessionForRestore(
        connectionId: UUID,
        entry: ESSWorkspaceSessionEntry,
        onReady: @escaping (SessionViewModel?) -> Void
    ) {
        pendingWorkspaceEntriesByConnectionId[connectionId] = entry
        pendingWorkspaceRestoreReadyByConnectionId[connectionId] = onReady
        connect(withId: connectionId)
        if let session = session(forConnectionId: connectionId) {
            resolvePendingWorkspaceRestore(connectionId: connectionId, session: session)
        }
    }

    private func openExplorer(_ kind: ExplorerKind) {
        guard let session = selectedSession else {
            status.notify(title: "Explorer", message: "Select a connected session first.", level: .warning)
            return
        }
        selectedConnectionId = session.connection.connectionId as UUID
        selectedSessionId = session.id
        session.ensureExplorersModel()
        session.explorers?.selectKind(kind)
        explorerDialogKind = kind
        activeModal = .explorer
    }

    private func buildActionRows(query: String) -> [PaletteRow] {
        _ = settingsEpoch
        let settings = ESSAppSettings.shared()
        var rows: [PaletteRow] = []
        for actionId in ESSAppSettings.shortcutActionIds() {
            let id = actionId as String
            if PaletteMetaActions.excluded.contains(id) { continue }
            let label = ESSAppSettings.shortcutLabel(forActionId: actionId)
            let group = ESSAppSettings.shortcutGroup(forActionId: actionId)
            let shortcut = settings.shortcut(forActionId: actionId) ?? ""
            let item = PaletteActionItem(
                actionId: id,
                label: label,
                group: group,
                shortcutText: KeySequence.displayString(fromPortable: shortcut),
                enabled: isPaletteActionEnabled(id)
            )
            let fields = [item.label, item.group, item.shortcutText, item.actionId]
            guard let score = FuzzyMatch.bestMatchScore(pattern: query, fields: fields) else { continue }
            rows.append(.action(item, score: score))
        }
        rows.sort { $0.score > $1.score }
        return rows
    }

    private func isPaletteActionEnabled(_ actionId: String) -> Bool {
        switch actionId {
        case "session.newSession":
            return selectedSession?.canOpenShell ?? false
        case "shell.close", "terminal.copy", "terminal.paste", "terminal.clearScreen",
             "terminal.search", "terminal.saveLog", "terminal.saveScreenshot":
            return canUseTerminalActions
        case "session.nextTab":
            return sessions.count >= 2
        case "session.previousTab":
            return sessions.count >= 2
        case "session.processExplorer", "session.containerExplorer", "session.serviceExplorer",
             "session.systemInfo":
            return selectedSession?.state == .connected
        default:
            return true
        }
    }

    private func buildConnectionRows(query: String) -> [PaletteRow] {
        library.reload()
        let recentIds = ESSAppSettings.shared().recentConnectionIds(withLimit: 8)
        var recentRank: [UUID: Int] = [:]
        for (index, id) in recentIds.enumerated() {
            if let uuid = id as UUID? {
                recentRank[uuid] = index
            }
        }

        var rows: [PaletteRow] = []
        for info in library.allConnections {
            let id = info.connectionId as UUID
            let name = info.name.isEmpty ? info.displayText : info.name
            let subtitle = "\(info.username)@\(info.host):\(info.port)"
            let fields = [name, subtitle, info.host, info.username, info.configAlias ?? "", String(info.port)]
            guard let baseScore = FuzzyMatch.bestMatchScore(pattern: query, fields: fields) else { continue }
            var score = baseScore
            if query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, let rank = recentRank[id] {
                score += 1000 - rank
            }
            let item = PaletteConnectionItem(
                connectionId: id,
                name: name,
                subtitle: subtitle,
                searchFields: fields,
                recentRank: recentRank[id] ?? -1
            )
            rows.append(.connection(item, score: score))
        }
        rows.sort { $0.score > $1.score }
        rows.append(.createConnection(score: Int.min))
        return rows
    }

    private func buildShellRows(query: String) -> [PaletteRow] {
        var rows: [PaletteRow] = []
        for session in sessions {
            let connectionId = session.connection.connectionId as UUID
            for shell in session.shells {
                let subtitle = session.title
                let fields = [shell.title, subtitle, session.title]
                guard var score = FuzzyMatch.bestMatchScore(pattern: query, fields: fields) else { continue }
                let isActive = session.focusedShellId == shell.id
                if query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, isActive {
                    score += 500
                }
                let item = PaletteShellItem(
                    connectionId: connectionId,
                    shellId: shell.id,
                    sessionTitle: session.title,
                    shellTitle: shell.title,
                    subtitle: subtitle,
                    searchFields: fields,
                    isActive: isActive
                )
                rows.append(.shell(item, score: score))
            }
        }
        if rows.isEmpty {
            return [.hint("No open shells. Connect and open a session first.")]
        }
        rows.sort { $0.score > $1.score }
        return rows
    }

    private func resolvePendingWorkspaceRestore(connectionId: UUID, session: SessionViewModel?) {
        guard let callback = pendingWorkspaceRestoreReadyByConnectionId.removeValue(forKey: connectionId) else {
            return
        }
        if session == nil {
            pendingWorkspaceEntriesByConnectionId.removeValue(forKey: connectionId)
        }
        callback(session)
    }

    private func selectExistingSession(for connectionId: UUID) -> Bool {
        guard let session = session(forConnectionId: connectionId) else { return false }
        selectedConnectionId = connectionId
        selectedSessionId = session.id
        sidebarMode = .sessions
        return true
    }

    private func syncSelectedConnectionFromSession() {
        guard let session = selectedSession else { return }
        selectedConnectionId = session.connection.connectionId as UUID
    }
}
