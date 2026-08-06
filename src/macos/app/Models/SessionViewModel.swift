// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import Combine
import Foundation
import SwiftUI
import UniformTypeIdentifiers

enum SessionUIState: String {
    case idle
    case connecting
    case connected
    case disconnected
    case failed
}

struct HostKeyPromptData: Identifiable {
    let id = UUID()
    let reason: ESSHostKeyPromptReason
    let fingerprint: String
    let contextLabel: String
}

struct ShellRenameRequest: Identifiable {
    let id = UUID()
    let shellId: UUID
    var title: String
}

@MainActor
final class SessionViewModel: ObservableObject, Identifiable {
    static let maxShells = 8

    let id = UUID()
    let connection: ESSConnectionInfo
    private var credentials: ESSSessionCredentials?

    @Published var title: String
    @Published var state: SessionUIState = .idle
    @Published var statusMessage: String = ""
    @Published var overlayMessage: String?
    @Published var showReconnect: Bool = false
    @Published var hostKeyPrompt: HostKeyPromptData?
    @Published var connectedAt: Date?

    @Published var shells: [ShellViewModel] = []
    @Published var focusedShellId: UUID?
    @Published var layout: ShellLayoutNode?
    @Published var renameRequest: ShellRenameRequest?
    @Published var pendingMultilinePaste: String?
    @Published var showFindBar: Bool = false
    /// Bumped when ESSAppSettings changes so terminals re-apply appearance.
    @Published var appearanceEpoch: UInt64 = 0

    /// Shell that should become AppKit first responder once its `TerminalView` exists.
    /// Cleared after `makeFirstResponder` succeeds (or focus moves elsewhere).
    private(set) var pendingTerminalActivationId: UUID?

    @Published var files: SessionFilesModel?
    @Published var explorers: SessionExplorersModel?
    @Published var tunnels: SessionTunnelsModel?

    /// Injected once after the next shell opens (service logs).
    private var pendingExplorerLogs: (shellId: UUID, command: String, title: String)?
    /// Shells opened for explorer logs — omitted from workspace capture.
    private var explorerAuxShellIds = Set<UUID>()
    /// Pending restore payload for this session (workspace Phase 8).
    var pendingWorkspaceRestore: ESSWorkspaceSessionEntry?
    private var workspaceRestoreBusy = false
    /// Called after workspace restore completes for one session (queue advance).
    var onWorkspaceRestoreFinished: (() -> Void)?

    /// App shell status sink (ErrorNotifier-style).
    var onStatus: ((String, StatusLevel) -> Void)?
    /// Fired once when the session becomes connected (for recent-connection tracking).
    var onConnectedOnce: ((UUID) -> Void)?
    /// Ask the app sheet host to present the host-key dialog (SessionPane sheets are unreliable).
    var onHostKeyPromptUI: ((HostKeyPromptData) -> Void)?

    private let controller = ESSSessionController()
    private var didRecordRecent = false
    private var shellSerial = 0
    private var shellCancellables = Set<AnyCancellable>()

    init(connection: ESSConnectionInfo, credentials: ESSSessionCredentials?) {
        self.connection = connection
        self.credentials = credentials
        let name = connection.name.trimmingCharacters(in: .whitespacesAndNewlines)
        self.title = name.isEmpty
            ? (connection.displayText.isEmpty
                ? "\(connection.username)@\(connection.host):\(connection.port)"
                : connection.displayText)
            : name
        wireController()
    }

    convenience init(draft: ConnectionDraft) {
        let form = ConnectionFormState.from(draft: draft)
        self.init(connection: form.makeConnectionInfo(), credentials: form.makeCredentials())
    }

    func refreshAppearance() {
        appearanceEpoch &+= 1
    }

    var focusedShell: ShellViewModel? {
        guard let focusedShellId else { return shells.first }
        return shells.first { $0.id == focusedShellId } ?? shells.first
    }

    var canOpenShell: Bool {
        state == .connected && shells.count < Self.maxShells
    }

    /// Workspace restore should only reopen live / in-flight sessions.
    var shouldPersistInWorkspace: Bool {
        switch state {
        case .connected, .connecting: return true
        case .idle, .disconnected, .failed: return false
        }
    }

    /// Fired when transport ends (disconnect / failure) so AppModel can persist workspace.
    var onTransportEnded: (() -> Void)?

    private func wireController() {
        controller.onConnected = { [weak self] shellId in
            Task { @MainActor in
                guard let self else { return }
                self.state = .connected
                self.connectedAt = Date()
                self.overlayMessage = nil
                self.showReconnect = false
                self.statusMessage = "Connected"
                self.onStatus?("Connected: \(self.title)", .success)
                self.resetShells()
                let initialId = shellId as UUID
                if let restore = self.pendingWorkspaceRestore {
                    self.addShell(id: initialId, focusAndLayout: false)
                    self.continueWorkspaceRestore(initialShellId: initialId)
                } else {
                    self.addShell(id: initialId, focusAndLayout: true)
                }
                self.files?.onSessionConnected()
                self.explorers?.onSessionConnected()
                self.tunnels?.onSessionConnected()
                if !self.didRecordRecent {
                    self.didRecordRecent = true
                    self.onConnectedOnce?(self.connection.connectionId as UUID)
                }
            }
        }
        controller.onShellOpened = { [weak self] shellId in
            Task { @MainActor in
                guard let self else { return }
                let id = shellId as UUID
                if self.shells.contains(where: { $0.id == id }) {
                    self.finishExplorerLogsIfNeeded(shellId: id)
                    return
                }
                self.addShell(id: id, focusAndLayout: true)
                self.finishExplorerLogsIfNeeded(shellId: id)
            }
        }
        controller.onData = { [weak self] shellId, data in
            Task { @MainActor in
                guard let self else { return }
                let id = shellId as UUID
                if let shell = self.shells.first(where: { $0.id == id }) {
                    shell.enqueueData(data)
                }
            }
        }
        controller.onShellClosed = { [weak self] shellId in
            Task { @MainActor in
                guard let self else { return }
                self.removeShellLocally(id: shellId as UUID, fromRemote: true)
            }
        }
        controller.onShellFailed = { [weak self] shellId, message in
            Task { @MainActor in
                guard let self else { return }
                let id = shellId as UUID
                self.onStatus?("Shell failed: \(message)", .error)
                self.removeShellLocally(id: id, fromRemote: true)
            }
        }
        controller.onHostKeyPrompt = { [weak self] reason, fingerprint, context in
            Task { @MainActor in
                guard let self else { return }
                let prompt = HostKeyPromptData(
                    reason: reason,
                    fingerprint: fingerprint,
                    contextLabel: context
                )
                self.hostKeyPrompt = prompt
                self.onHostKeyPromptUI?(prompt)
            }
        }
        controller.onError = { [weak self] message in
            Task { @MainActor in
                guard let self else { return }
                self.state = .failed
                self.connectedAt = nil
                self.overlayMessage = message
                self.showReconnect = true
                self.statusMessage = message
                self.onStatus?("Failed: \(self.title)", .error)
                self.resetShells()
                self.files?.onSessionDisconnected()
                self.explorers?.onSessionDisconnected()
                self.tunnels?.onSessionDisconnected()
                if self.pendingWorkspaceRestore != nil {
                    self.pendingWorkspaceRestore = nil
                    self.onWorkspaceRestoreFinished?()
                }
                self.onTransportEnded?()
            }
        }
        controller.onDisconnected = { [weak self] in
            Task { @MainActor in
                guard let self else { return }
                self.state = .disconnected
                self.connectedAt = nil
                self.overlayMessage = "Disconnected from \(self.title)."
                self.showReconnect = true
                self.statusMessage = "Disconnected"
                self.onStatus?("Disconnected: \(self.title)", .warning)
                self.resetShells()
                self.files?.onSessionDisconnected()
                self.explorers?.onSessionDisconnected()
                self.tunnels?.onSessionDisconnected()
                self.onTransportEnded?()
            }
        }
        controller.onAgentForwardingWarning = { [weak self] message in
            Task { @MainActor in
                guard let self else { return }
                self.statusMessage = message
                self.onStatus?(message, .warning)
            }
        }
    }

    func connect() {
        state = .connecting
        connectedAt = nil
        overlayMessage = "Connecting…"
        showReconnect = false
        statusMessage = "Connecting…"
        let cols = focusedShell?.cols ?? 80
        let rows = focusedShell?.rows ?? 24
        controller.connect(
            withConnection: connection,
            credentials: credentials,
            cols: cols,
            rows: rows
        )
    }

    func disconnect() {
        // Mark ended before the async worker callback so workspace save/quit
        // cannot race and persist this session again.
        if state == .connected || state == .connecting {
            state = .disconnected
            connectedAt = nil
            overlayMessage = "Disconnected from \(title)."
            showReconnect = true
            statusMessage = "Disconnected"
        }
        controller.disconnect()
    }

    func reconnect() {
        state = .connecting
        connectedAt = nil
        overlayMessage = "Connecting…"
        showReconnect = false
        resetShells()
        let cols = 80
        let rows = 24
        controller.reconnect(withCols: cols, rows: rows)
    }

    func respondHostKey(accept: Bool) {
        hostKeyPrompt = nil
        controller.respondHostKeyTrust(accept)
        if !accept {
            state = .failed
            connectedAt = nil
            overlayMessage = "Host key rejected."
            showReconnect = true
            onStatus?("Host key rejected: \(title)", .warning)
            resetShells()
        }
    }

    // MARK: - Shell management

    func openShell(id shellId: UUID, title: String? = nil) {
        guard canOpenShell else { return }
        let cols = focusedShell?.cols ?? 80
        let rows = focusedShell?.rows ?? 24
        addShell(id: shellId, focusAndLayout: false)
        if let title, !title.isEmpty {
            shells.first { $0.id == shellId }?.rename(title)
        }
        controller.openShell(shellId, cols: cols, rows: rows)
    }

    func openShell() {
        guard canOpenShell else {
            if shells.count >= Self.maxShells {
                onStatus?("Maximum of \(Self.maxShells) shells per session", .warning)
            }
            return
        }
        let shellId = UUID()
        let cols = focusedShell?.cols ?? 80
        let rows = focusedShell?.rows ?? 24
        // Optimistic local entry; bridge also emits onShellOpened.
        addShell(id: shellId, focusAndLayout: true)
        controller.openShell(shellId, cols: cols, rows: rows)
    }

    /// Opens a new shell and injects a follow-logs command (service explorer).
    func openShellForExplorerLogs(title: String, command: String) {
        guard canOpenShell else {
            onStatus?("Cannot open logs shell (session busy or at shell limit).", .warning)
            return
        }
        let shellId = UUID()
        let cols = focusedShell?.cols ?? 80
        let rows = focusedShell?.rows ?? 24
        pendingExplorerLogs = (shellId, command, title)
        explorerAuxShellIds.insert(shellId)
        addShell(id: shellId, focusAndLayout: true)
        shells.first { $0.id == shellId }?.rename(title)
        controller.openShell(shellId, cols: cols, rows: rows)
    }

    private func finishExplorerLogsIfNeeded(shellId: UUID) {
        guard let pending = pendingExplorerLogs, pending.shellId == shellId else { return }
        pendingExplorerLogs = nil
        shells.first { $0.id == shellId }?.rename(pending.title)
        if let data = pending.command.data(using: .utf8) {
            // Slight delay so the remote PTY is ready.
            Task { @MainActor in
                try? await Task.sleep(nanoseconds: 250_000_000)
                self.sendTerminalData(data, shellId: shellId)
            }
        }
    }

    func closeShell(_ shellId: UUID) {
        guard shells.contains(where: { $0.id == shellId }) else { return }
        if shells.count <= 1 {
            // Closing the last shell tears down transport UX like Qt close-last.
            controller.closeShell(shellId)
            removeShellLocally(id: shellId, fromRemote: false)
            overlayMessage = "Shell closed. Reconnect to open a new shell."
            showReconnect = true
            statusMessage = "Shell closed"
            connectedAt = nil
            onStatus?("Shell closed: \(title)", .warning)
            return
        }
        controller.closeShell(shellId)
        removeShellLocally(id: shellId, fromRemote: false)
    }

    func closeFocusedShell() {
        guard let id = focusedShellId ?? shells.first?.id else { return }
        closeShell(id)
    }

    /// Marks `shellId` as the session's focused shell and optionally transfers AppKit key focus
    /// to its SwiftTerm view (so only that caret blinks; others stay steady/visible).
    func focusShell(_ shellId: UUID, activateTerminal: Bool = true) {
        guard shells.contains(where: { $0.id == shellId }) else { return }
        let previous = focusedShellId
        focusedShellId = shellId
        guard var tree = layout else {
            layout = .leaf(shellId)
            if activateTerminal {
                requestTerminalActivation(for: shellId)
            } else {
                syncTerminalCursors(activeId: shellId)
            }
            return
        }
        if tree.contains(shellId) {
            objectWillChange.send()
            if activateTerminal {
                requestTerminalActivation(for: shellId)
            } else {
                syncTerminalCursors(activeId: shellId)
            }
            return
        }
        // Shell not in visible tree (overflow past max panes / smart layout off): swap into focus leaf.
        if let previous, tree.contains(previous) {
            tree = tree.replacing(previous, with: shellId)
            layout = tree
        } else if let first = tree.leafIds.first {
            tree = tree.replacing(first, with: shellId)
            layout = tree
        } else {
            layout = .leaf(shellId)
        }
        if activateTerminal {
            requestTerminalActivation(for: shellId)
        } else {
            syncTerminalCursors(activeId: shellId)
        }
    }

    /// Queue AppKit first-responder transfer. Safe to call before the `TerminalView` exists;
    /// `TerminalRepresentable.updateNSView` / a deferred flush will complete it.
    func requestTerminalActivation(for shellId: UUID) {
        guard focusedShellId == shellId else { return }
        pendingTerminalActivationId = shellId
        DispatchQueue.main.async { [weak self] in
            self?.flushPendingTerminalActivation()
        }
    }

    /// Make the pending terminal the window's first responder.
    /// Returns `true` when activation completed or is no longer needed.
    @discardableResult
    func flushPendingTerminalActivation() -> Bool {
        guard let shellId = pendingTerminalActivationId else { return true }
        guard focusedShellId == shellId else {
            pendingTerminalActivationId = nil
            return true
        }
        // Keep pending while the find field owns key focus; close-find re-requests.
        if showFindBar {
            return false
        }
        guard let terminal = shells.first(where: { $0.id == shellId })?.terminalView,
              let window = terminal.window
        else {
            return false
        }
        pendingTerminalActivationId = nil
        if window.firstResponder !== terminal {
            window.makeFirstResponder(terminal)
        }
        syncTerminalCursors(activeId: shellId)
        return true
    }

    /// Ensure only the focused shell's caret blinks; all others stay steady/visible.
    func syncTerminalCursors(activeId: UUID? = nil) {
        let active = activeId ?? focusedShellId
        for shell in shells {
            guard let terminal = shell.terminalView else { continue }
            TerminalAppearance.applyCursor(to: terminal, active: shell.id == active)
        }
    }

    func beginRenameShell(_ shellId: UUID) {
        guard let shell = shells.first(where: { $0.id == shellId }) else { return }
        renameRequest = ShellRenameRequest(shellId: shellId, title: shell.title)
    }

    func beginRenameFocusedShell() {
        guard let id = focusedShellId ?? shells.first?.id else { return }
        beginRenameShell(id)
    }

    func commitRename(request: ShellRenameRequest) {
        renameRequest = nil
        shells.first { $0.id == request.shellId }?.rename(request.title)
    }

    func sendTerminalData(_ data: Data, shellId: UUID?) {
        guard state == .connected else { return }
        let target = shellId ?? focusedShellId
        controller.write(data, shellId: target)
    }

    func resizeTerminal(cols: Int, rows: Int, shellId: UUID) {
        guard let shell = shells.first(where: { $0.id == shellId }) else { return }
        let nextCols = max(cols, 2)
        let nextRows = max(rows, 2)
        let sizeChanged = shell.cols != nextCols || shell.rows != nextRows
        shell.cols = nextCols
        shell.rows = nextRows
        guard state == .connected, sizeChanged else { return }
        controller.resizeCols(Int(shell.cols), rows: Int(shell.rows), shellId: shellId)
    }

    func pasteClipboard() {
        guard let str = NSPasteboard.general.string(forType: .string), !str.isEmpty else { return }
        let confirm = ESSAppSettings.shared().confirmMultilinePaste
        if confirm, str.contains(where: { $0 == "\n" || $0 == "\r" }) {
            pendingMultilinePaste = str
            return
        }
        sendPaste(str)
    }

    func confirmMultilinePaste() {
        guard let str = pendingMultilinePaste else { return }
        pendingMultilinePaste = nil
        sendPaste(str)
    }

    func cancelMultilinePaste() {
        pendingMultilinePaste = nil
    }

    func copySelection() {
        focusedShell?.copySelection()
    }

    func clearFocusedTerminal() {
        focusedShell?.clearScreen()
    }

    func toggleFindBar() {
        showFindBar.toggle()
        if let shell = focusedShell {
            shell.showFindBar = showFindBar
            if !showFindBar {
                shell.clearFind()
                requestTerminalActivation(for: shell.id)
            } else {
                pendingTerminalActivationId = nil
            }
        }
    }

    func saveLogForFocusedShell() {
        guard let shell = focusedShell else { return }
        let text = shell.bufferText()
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.plainText]
        panel.nameFieldStringValue = sanitizeFileBaseName(shell.title) + ".log"
        panel.canCreateDirectories = true
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            do {
                try text.write(to: url, atomically: true, encoding: .utf8)
                Task { @MainActor in
                    self?.onStatus?("Saved log: \(url.lastPathComponent)", .success)
                }
            } catch {
                Task { @MainActor in
                    self?.onStatus?("Could not save log: \(error.localizedDescription)", .error)
                }
            }
        }
    }

    func saveScreenshotForFocusedShell() {
        guard let shell = focusedShell, let view = shell.terminalView else {
            onStatus?("No terminal to capture", .warning)
            return
        }
        guard let rep = view.bitmapImageRepForCachingDisplay(in: view.bounds) else {
            onStatus?("Could not capture terminal", .warning)
            return
        }
        view.cacheDisplay(in: view.bounds, to: rep)
        let image = NSImage(size: view.bounds.size)
        image.addRepresentation(rep)
        guard let tiff = image.tiffRepresentation,
              let bitmap = NSBitmapImageRep(data: tiff),
              let png = bitmap.representation(using: .png, properties: [:])
        else {
            onStatus?("Could not encode screenshot", .warning)
            return
        }
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.png]
        panel.nameFieldStringValue = sanitizeFileBaseName(shell.title) + ".png"
        panel.canCreateDirectories = true
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            do {
                try png.write(to: url)
                Task { @MainActor in
                    self?.onStatus?("Saved screenshot: \(url.lastPathComponent)", .success)
                }
            } catch {
                Task { @MainActor in
                    self?.onStatus?("Could not save screenshot: \(error.localizedDescription)", .error)
                }
            }
        }
    }

    func beginWorkspaceRestore(_ entry: ESSWorkspaceSessionEntry) {
        pendingWorkspaceRestore = entry
    }

    func captureWorkspaceEntry() -> ESSWorkspaceSessionEntry {
        let entry = ESSWorkspaceSessionEntry()
        entry.connectionId = connection.connectionId as UUID
        var shellEntries: [ESSWorkspaceShellEntry] = []
        for shell in shells where !explorerAuxShellIds.contains(shell.id) {
            let spec = ESSWorkspaceShellEntry()
            spec.shellId = shell.id
            spec.title = shell.title
            shellEntries.append(spec)
        }
        entry.shells = shellEntries
        if let focusedShellId {
            entry.activeShellId = focusedShellId
        }
        if let explorers {
            entry.activeToolId = explorers.selectedKind.bridgeKind
        } else {
            entry.activeToolId = ""
        }
        entry.tools = []
        entry.dockState = ShellLayoutCodec.encode(layout)
        return entry
    }

    private func continueWorkspaceRestore(initialShellId: UUID) {
        guard !workspaceRestoreBusy, let entry = pendingWorkspaceRestore else { return }
        workspaceRestoreBusy = true

        let savedIds = Set(entry.shells.compactMap { $0.shellId as UUID? })
        if !savedIds.isEmpty, !savedIds.contains(initialShellId) {
            controller.closeShell(initialShellId)
            removeShellLocally(id: initialShellId, fromRemote: false)
        }

        var alive = Set(shells.map(\.id))
        for spec in entry.shells {
            guard let id = spec.shellId as UUID? else { continue }
            if alive.contains(id) { continue }
            openShell(id: id, title: spec.title)
            alive.insert(id)
        }

        for spec in entry.shells {
            guard let id = spec.shellId as UUID?, !spec.title.isEmpty else { continue }
            shells.first { $0.id == id }?.rename(spec.title)
        }

        if let data = entry.dockState as Data?, let restored = ShellLayoutCodec.decode(data) {
            layout = restored
        } else if layout == nil, let first = shells.first?.id {
            layout = .leaf(first)
        }

        let toolId = entry.activeToolId ?? ""
        if !toolId.isEmpty, let kind = ExplorerKind(rawValue: toolId) {
            ensureExplorersModel()
            explorers?.selectKind(kind)
            onStatus?("Restored explorer: \(kind.title)", .status)
        }

        if let activeShell = entry.activeShellId as UUID?, shells.contains(where: { $0.id == activeShell }) {
            focusShell(activeShell)
        } else if let first = shells.first?.id {
            focusShell(first)
        }

        pendingWorkspaceRestore = nil
        workspaceRestoreBusy = false
        onWorkspaceRestoreFinished?()
    }

    // MARK: - Extension points (ready for Files / Tunnels UI)

    func ensureFilesModel() {
        if files == nil {
            let model = SessionFilesModel()
            files = model
            model.attach(to: self)
        } else {
            files?.attach(to: self)
        }
    }

    func ensureExplorersModel() {
        if explorers == nil {
            let model = SessionExplorersModel()
            explorers = model
            model.attach(to: self)
        } else {
            explorers?.attach(to: self)
        }
    }

    func ensureTunnelsModel() {
        if tunnels == nil {
            let model = SessionTunnelsModel()
            tunnels = model
            model.attach(to: self)
        } else {
            tunnels?.attach(to: self)
        }
    }

    var sessionController: ESSSessionController { controller }

    // MARK: - Private

    private func sendPaste(_ str: String) {
        guard let data = str.data(using: .utf8) else { return }
        sendTerminalData(data, shellId: focusedShellId)
    }

    private func resetShells() {
        shellCancellables.removeAll()
        shells = []
        focusedShellId = nil
        layout = nil
        showFindBar = false
        renameRequest = nil
        pendingMultilinePaste = nil
        shellSerial = 0
        explorerAuxShellIds.removeAll()
    }

    private func addShell(id: UUID, focusAndLayout: Bool) {
        if shells.contains(where: { $0.id == id }) {
            if focusAndLayout {
                focusShell(id)
            }
            return
        }
        shellSerial += 1
        let shell = ShellViewModel(id: id, title: "Shell \(shellSerial)")
        shell.objectWillChange
            .sink { [weak self] _ in
                self?.objectWillChange.send()
            }
            .store(in: &shellCancellables)
        shells.append(shell)
        if focusAndLayout {
            placeShellInLayout(id)
            focusedShellId = id
            requestTerminalActivation(for: id)
        }
    }

    private func placeShellInLayout(_ newId: UUID) {
        let smart = ESSAppSettings.shared().smartLayout
        guard var tree = layout else {
            layout = .leaf(newId)
            return
        }

        if !smart {
            if let focus = focusedShellId, tree.contains(focus) {
                tree = tree.replacing(focus, with: newId)
                layout = tree
            } else {
                layout = .leaf(newId)
            }
            return
        }

        let leafCount = tree.leafCount
        if leafCount >= ShellLayoutPlanner.maxVisibleLeaves {
            if let focus = focusedShellId, tree.contains(focus) {
                tree = tree.replacing(focus, with: newId)
                layout = tree
            } else if let first = tree.leafIds.first {
                tree = tree.replacing(first, with: newId)
                layout = tree
            } else {
                layout = .leaf(newId)
            }
            return
        }

        let relative = focusedShellId.flatMap { tree.contains($0) ? $0 : nil }
            ?? tree.leafIds.first
            ?? newId
        if relative == newId, leafCount == 0 {
            layout = .leaf(newId)
            return
        }
        let axis = ShellLayoutPlanner.axisForNewShell(currentLeafCount: leafCount)
        layout = tree.splitting(relativeTo: relative, newId: newId, axis: axis)
    }

    private func removeShellLocally(id: UUID, fromRemote: Bool) {
        shells.removeAll { $0.id == id }
        if let tree = layout {
            layout = tree.removing(id)
        }
        if focusedShellId == id {
            focusedShellId = shells.last?.id
            if let focus = focusedShellId, let tree = layout, !tree.contains(focus) {
                if let first = tree.leafIds.first {
                    layout = tree.replacing(first, with: focus)
                } else {
                    layout = .leaf(focus)
                }
            }
        }
        if shells.isEmpty, state == .connected, fromRemote {
            overlayMessage = "Shell closed. Reconnect to open a new shell."
            showReconnect = true
            statusMessage = "Shell closed"
            connectedAt = nil
            onStatus?("Shell closed: \(title)", .warning)
        }
    }

    private func sanitizeFileBaseName(_ name: String) -> String {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let cleaned = trimmed.replacingOccurrences(
            of: #"[\\/:*?"<>|]+"#,
            with: "_",
            options: .regularExpression
        )
        return cleaned.isEmpty ? "terminal" : cleaned
    }
}
