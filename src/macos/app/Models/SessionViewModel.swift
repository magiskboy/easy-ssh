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

struct TerminalRenameRequest: Identifiable {
    let id = UUID()
    let terminalId: UUID
    var title: String
}

@MainActor
final class SessionViewModel: ObservableObject, Identifiable {
    static let maxTerminals = 8

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

    @Published var terminals: [TerminalViewModel] = []
    @Published var focusedTerminalId: UUID?
    @Published var layout: TerminalLayoutNode?
    @Published var renameRequest: TerminalRenameRequest?
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
    private var pendingExplorerLogs: (terminalId: UUID, command: String, title: String)?
    /// Terminals opened for explorer logs — omitted from workspace capture.
    private var explorerAuxTerminalIds = Set<UUID>()
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
    private var terminalSerial = 0
    private var terminalCancellables = Set<AnyCancellable>()

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

    var focusedTerminal: TerminalViewModel? {
        guard let focusedTerminalId else { return terminals.first }
        return terminals.first { $0.id == focusedTerminalId } ?? terminals.first
    }

    var canOpenTerminal: Bool {
        state == .connected && terminals.count < Self.maxTerminals
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
        controller.onConnected = { [weak self] terminalId in
            Task { @MainActor in
                guard let self else { return }
                self.state = .connected
                self.connectedAt = Date()
                self.overlayMessage = nil
                self.showReconnect = false
                self.statusMessage = "Connected"
                self.onStatus?("Connected: \(self.title)", .success)
                self.resetTerminals()
                let initialId = terminalId as UUID
                if let restore = self.pendingWorkspaceRestore {
                    self.addTerminal(id: initialId, focusAndLayout: false)
                    self.continueWorkspaceRestore(initialTerminalId: initialId)
                } else {
                    self.addTerminal(id: initialId, focusAndLayout: true)
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
        controller.onTerminalOpened = { [weak self] terminalId in
            Task { @MainActor in
                guard let self else { return }
                let id = terminalId as UUID
                if self.terminals.contains(where: { $0.id == id }) {
                    self.finishExplorerLogsIfNeeded(terminalId: id)
                    return
                }
                self.addTerminal(id: id, focusAndLayout: true)
                self.finishExplorerLogsIfNeeded(terminalId: id)
            }
        }
        controller.onData = { [weak self] terminalId, data in
            Task { @MainActor in
                guard let self else { return }
                let id = terminalId as UUID
                if let shell = self.terminals.first(where: { $0.id == id }) {
                    shell.enqueueData(data)
                }
            }
        }
        controller.onTerminalClosed = { [weak self] terminalId in
            Task { @MainActor in
                guard let self else { return }
                self.removeTerminalLocally(id: terminalId as UUID, fromRemote: true)
            }
        }
        controller.onTerminalFailed = { [weak self] terminalId, message in
            Task { @MainActor in
                guard let self else { return }
                let id = terminalId as UUID
                self.onStatus?("Terminal failed: \(message)", .error)
                self.removeTerminalLocally(id: id, fromRemote: true)
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
                self.resetTerminals()
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
                self.resetTerminals()
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
        let cols = focusedTerminal?.cols ?? 80
        let rows = focusedTerminal?.rows ?? 24
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
        resetTerminals()
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
            resetTerminals()
        }
    }

    // MARK: - Terminal management

    func openTerminal(id terminalId: UUID, title: String? = nil) {
        guard canOpenTerminal else { return }
        let cols = focusedTerminal?.cols ?? 80
        let rows = focusedTerminal?.rows ?? 24
        addTerminal(id: terminalId, focusAndLayout: false)
        if let title, !title.isEmpty {
            terminals.first { $0.id == terminalId }?.rename(title)
        }
        controller.openTerminal(terminalId, cols: cols, rows: rows)
    }

    func openTerminal() {
        guard canOpenTerminal else {
            if terminals.count >= Self.maxTerminals {
                onStatus?("Maximum of \(Self.maxTerminals) terminals per session", .warning)
            }
            return
        }
        let terminalId = UUID()
        let cols = focusedTerminal?.cols ?? 80
        let rows = focusedTerminal?.rows ?? 24
        // Optimistic local entry; bridge also emits onTerminalOpened.
        addTerminal(id: terminalId, focusAndLayout: true)
        controller.openTerminal(terminalId, cols: cols, rows: rows)
    }

    /// Opens a new shell and injects a follow-logs command (service explorer).
    func openTerminalForExplorerLogs(title: String, command: String) {
        guard canOpenTerminal else {
            onStatus?("Cannot open logs terminal (session busy or at terminal limit).", .warning)
            return
        }
        let terminalId = UUID()
        let cols = focusedTerminal?.cols ?? 80
        let rows = focusedTerminal?.rows ?? 24
        pendingExplorerLogs = (terminalId, command, title)
        explorerAuxTerminalIds.insert(terminalId)
        addTerminal(id: terminalId, focusAndLayout: true)
        terminals.first { $0.id == terminalId }?.rename(title)
        controller.openTerminal(terminalId, cols: cols, rows: rows)
    }

    private func finishExplorerLogsIfNeeded(terminalId: UUID) {
        guard let pending = pendingExplorerLogs, pending.terminalId == terminalId else { return }
        pendingExplorerLogs = nil
        terminals.first { $0.id == terminalId }?.rename(pending.title)
        if let data = pending.command.data(using: .utf8) {
            // Slight delay so the remote PTY is ready.
            Task { @MainActor in
                try? await Task.sleep(nanoseconds: 250_000_000)
                self.sendTerminalData(data, terminalId: terminalId)
            }
        }
    }

    func closeTerminal(_ terminalId: UUID) {
        guard terminals.contains(where: { $0.id == terminalId }) else { return }
        if terminals.count <= 1 {
            // Closing the last shell tears down transport UX like Qt close-last.
            controller.closeTerminal(terminalId)
            removeTerminalLocally(id: terminalId, fromRemote: false)
            overlayMessage = "Terminal closed. Reconnect to open a new terminal."
            showReconnect = true
            statusMessage = "Terminal closed"
            connectedAt = nil
            onStatus?("Terminal closed: \(title)", .warning)
            return
        }
        controller.closeTerminal(terminalId)
        removeTerminalLocally(id: terminalId, fromRemote: false)
    }

    func closeFocusedTerminal() {
        guard let id = focusedTerminalId ?? terminals.first?.id else { return }
        closeTerminal(id)
    }

    /// Marks `terminalId` as the session's focused shell and optionally transfers AppKit key focus
    /// to its SwiftTerm view (so only that caret blinks; others stay steady/visible).
    func focusTerminal(_ terminalId: UUID, activateTerminal: Bool = true) {
        guard terminals.contains(where: { $0.id == terminalId }) else { return }
        let previous = focusedTerminalId
        focusedTerminalId = terminalId
        guard var tree = layout else {
            layout = .leaf(terminalId)
            if activateTerminal {
                requestTerminalActivation(for: terminalId)
            } else {
                syncTerminalCursors(activeId: terminalId)
            }
            return
        }
        if tree.contains(terminalId) {
            objectWillChange.send()
            if activateTerminal {
                requestTerminalActivation(for: terminalId)
            } else {
                syncTerminalCursors(activeId: terminalId)
            }
            return
        }
        // Shell not in visible tree (overflow past max panes / smart layout off): swap into focus leaf.
        if let previous, tree.contains(previous) {
            tree = tree.replacing(previous, with: terminalId)
            layout = tree
        } else if let first = tree.leafIds.first {
            tree = tree.replacing(first, with: terminalId)
            layout = tree
        } else {
            layout = .leaf(terminalId)
        }
        if activateTerminal {
            requestTerminalActivation(for: terminalId)
        } else {
            syncTerminalCursors(activeId: terminalId)
        }
    }

    /// Queue AppKit first-responder transfer. Safe to call before the `TerminalView` exists;
    /// `TerminalRepresentable.updateNSView` / a deferred flush will complete it.
    func requestTerminalActivation(for terminalId: UUID) {
        guard focusedTerminalId == terminalId else { return }
        pendingTerminalActivationId = terminalId
        DispatchQueue.main.async { [weak self] in
            self?.flushPendingTerminalActivation()
        }
    }

    /// Make the pending terminal the window's first responder.
    /// Returns `true` when activation completed or is no longer needed.
    @discardableResult
    func flushPendingTerminalActivation() -> Bool {
        guard let terminalId = pendingTerminalActivationId else { return true }
        guard focusedTerminalId == terminalId else {
            pendingTerminalActivationId = nil
            return true
        }
        // Keep pending while the find field owns key focus; close-find re-requests.
        if showFindBar {
            return false
        }
        guard let terminal = terminals.first(where: { $0.id == terminalId })?.terminalView,
              let window = terminal.window
        else {
            return false
        }
        pendingTerminalActivationId = nil
        if window.firstResponder !== terminal {
            window.makeFirstResponder(terminal)
        }
        syncTerminalCursors(activeId: terminalId)
        return true
    }

    /// Ensure only the focused shell's caret blinks; all others stay steady/visible.
    func syncTerminalCursors(activeId: UUID? = nil) {
        let active = activeId ?? focusedTerminalId
        for shell in terminals {
            guard let terminal = shell.terminalView else { continue }
            TerminalAppearance.applyCursor(to: terminal, active: shell.id == active)
        }
    }

    func beginRenameTerminal(_ terminalId: UUID) {
        guard let shell = terminals.first(where: { $0.id == terminalId }) else { return }
        renameRequest = TerminalRenameRequest(terminalId: terminalId, title: shell.title)
    }

    func beginRenameFocusedTerminal() {
        guard let id = focusedTerminalId ?? terminals.first?.id else { return }
        beginRenameTerminal(id)
    }

    func commitRename(request: TerminalRenameRequest) {
        renameRequest = nil
        terminals.first { $0.id == request.terminalId }?.rename(request.title)
    }

    func sendTerminalData(_ data: Data, terminalId: UUID?) {
        guard state == .connected else { return }
        let target = terminalId ?? focusedTerminalId
        controller.write(data, terminalId: target)
    }

    func resizeTerminal(cols: Int, rows: Int, terminalId: UUID) {
        guard let shell = terminals.first(where: { $0.id == terminalId }) else { return }
        let nextCols = max(cols, 2)
        let nextRows = max(rows, 2)
        let sizeChanged = shell.cols != nextCols || shell.rows != nextRows
        shell.cols = nextCols
        shell.rows = nextRows
        guard state == .connected, sizeChanged else { return }
        controller.resizeCols(Int(shell.cols), rows: Int(shell.rows), terminalId: terminalId)
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
        focusedTerminal?.copySelection()
    }

    func clearFocusedTerminal() {
        focusedTerminal?.clearScreen()
    }

    func toggleFindBar() {
        showFindBar.toggle()
        if let shell = focusedTerminal {
            shell.showFindBar = showFindBar
            if !showFindBar {
                shell.clearFind()
                requestTerminalActivation(for: shell.id)
            } else {
                pendingTerminalActivationId = nil
            }
        }
    }

    func saveLogForFocusedTerminal() {
        guard let shell = focusedTerminal else { return }
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

    func saveScreenshotForFocusedTerminal() {
        guard let shell = focusedTerminal, let view = shell.terminalView else {
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
        var terminalEntries: [ESSWorkspaceTerminalEntry] = []
        for shell in terminals where !explorerAuxTerminalIds.contains(shell.id) {
            let spec = ESSWorkspaceTerminalEntry()
            spec.terminalId = shell.id
            spec.title = shell.title
            terminalEntries.append(spec)
        }
        entry.terminals = terminalEntries
        if let focusedTerminalId {
            entry.activeTerminalId = focusedTerminalId
        }
        if let explorers {
            entry.activeToolId = explorers.selectedKind.bridgeKind
        } else {
            entry.activeToolId = ""
        }
        entry.tools = []
        entry.dockState = TerminalLayoutCodec.encode(layout)
        return entry
    }

    private func continueWorkspaceRestore(initialTerminalId: UUID) {
        guard !workspaceRestoreBusy, let entry = pendingWorkspaceRestore else { return }
        workspaceRestoreBusy = true

        let savedIds = Set(entry.terminals.compactMap { $0.terminalId as UUID? })
        if !savedIds.isEmpty, !savedIds.contains(initialTerminalId) {
            controller.closeTerminal(initialTerminalId)
            removeTerminalLocally(id: initialTerminalId, fromRemote: false)
        }

        var alive = Set(terminals.map(\.id))
        for spec in entry.terminals {
            guard let id = spec.terminalId as UUID? else { continue }
            if alive.contains(id) { continue }
            openTerminal(id: id, title: spec.title)
            alive.insert(id)
        }

        for spec in entry.terminals {
            guard let id = spec.terminalId as UUID?, !spec.title.isEmpty else { continue }
            terminals.first { $0.id == id }?.rename(spec.title)
        }

        if let data = entry.dockState as Data?, let restored = TerminalLayoutCodec.decode(data) {
            layout = restored
        } else if layout == nil, let first = terminals.first?.id {
            layout = .leaf(first)
        }

        let toolId = entry.activeToolId ?? ""
        if !toolId.isEmpty, let kind = ExplorerKind(rawValue: toolId) {
            ensureExplorersModel()
            explorers?.selectKind(kind)
            onStatus?("Restored explorer: \(kind.title)", .status)
        }

        if let activeShell = entry.activeTerminalId as UUID?, terminals.contains(where: { $0.id == activeShell }) {
            focusTerminal(activeShell)
        } else if let first = terminals.first?.id {
            focusTerminal(first)
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
        sendTerminalData(data, terminalId: focusedTerminalId)
    }

    private func resetTerminals() {
        for shell in terminals {
            shell.releaseTerminal()
        }
        terminalCancellables.removeAll()
        terminals = []
        focusedTerminalId = nil
        layout = nil
        showFindBar = false
        renameRequest = nil
        pendingMultilinePaste = nil
        terminalSerial = 0
        explorerAuxTerminalIds.removeAll()
    }

    private func addTerminal(id: UUID, focusAndLayout: Bool) {
        if terminals.contains(where: { $0.id == id }) {
            if focusAndLayout {
                focusTerminal(id)
            }
            return
        }
        terminalSerial += 1
        let shell = TerminalViewModel(id: id, title: "Terminal \(terminalSerial)")
        shell.objectWillChange
            .sink { [weak self] _ in
                self?.objectWillChange.send()
            }
            .store(in: &terminalCancellables)
        terminals.append(shell)
        if focusAndLayout {
            placeTerminalInLayout(id)
            focusedTerminalId = id
            requestTerminalActivation(for: id)
        }
    }

    private func placeTerminalInLayout(_ newId: UUID) {
        let smart = ESSAppSettings.shared().smartLayout
        guard var tree = layout else {
            layout = .leaf(newId)
            return
        }

        if !smart {
            if let focus = focusedTerminalId, tree.contains(focus) {
                tree = tree.replacing(focus, with: newId)
                layout = tree
            } else {
                layout = .leaf(newId)
            }
            return
        }

        let leafCount = tree.leafCount
        if leafCount >= TerminalLayoutPlanner.maxVisibleLeaves {
            if let focus = focusedTerminalId, tree.contains(focus) {
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

        let relative = focusedTerminalId.flatMap { tree.contains($0) ? $0 : nil }
            ?? tree.leafIds.first
            ?? newId
        if relative == newId, leafCount == 0 {
            layout = .leaf(newId)
            return
        }
        let axis = TerminalLayoutPlanner.axisForNewTerminal(currentLeafCount: leafCount)
        layout = tree.splitting(relativeTo: relative, newId: newId, axis: axis)
    }

    private func removeTerminalLocally(id: UUID, fromRemote: Bool) {
        if let shell = terminals.first(where: { $0.id == id }) {
            shell.releaseTerminal()
        }
        terminals.removeAll { $0.id == id }
        if let tree = layout {
            layout = tree.removing(id)
        }

        // Closing the only visible leaf collapses layout to nil (common with smart
        // layout off). Promote a remaining shell into focus + layout so the UI
        // does not flash "No Terminal" while terminals.isEmpty is still false.
        let focusStillValid = focusedTerminalId.map { fid in terminals.contains(where: { $0.id == fid }) } ?? false
        if !focusStillValid {
            focusedTerminalId = terminals.last?.id
        }
        if let focus = focusedTerminalId {
            if layout == nil {
                layout = .leaf(focus)
            } else if let tree = layout, !tree.contains(focus) {
                if let first = tree.leafIds.first {
                    layout = tree.replacing(first, with: focus)
                } else {
                    layout = .leaf(focus)
                }
            }
            requestTerminalActivation(for: focus)
        } else {
            layout = nil
        }

        if terminals.isEmpty, state == .connected, fromRemote {
            overlayMessage = "Terminal closed. Reconnect to open a new terminal."
            showReconnect = true
            statusMessage = "Terminal closed"
            connectedAt = nil
            onStatus?("Terminal closed: \(title)", .warning)
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
