// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import Combine
import Foundation
import SwiftUI

enum SessionUIState: String {
    case idle
    case connecting
    case connected
    case disconnected
    case failed
}

/// Reserved extension points for later file explorer / tunnels UI on the same controller.
final class SessionFilesModel: ObservableObject {}
final class SessionTunnelsModel: ObservableObject {}

@MainActor
final class SessionViewModel: ObservableObject, Identifiable {
    let id = UUID()
    let connection: ESSConnectionInfo
    private var credentials: ESSSessionCredentials?

    @Published var title: String
    @Published var state: SessionUIState = .idle
    @Published var statusMessage: String = ""
    @Published var overlayMessage: String?
    @Published var showReconnect: Bool = false
    @Published var hostKeyPrompt: HostKeyPromptData?
    @Published var terminalFeedToken: UInt64 = 0
    @Published var pendingTerminalData: Data?
    @Published var lastCols: Int = 80
    @Published var lastRows: Int = 24
    @Published var connectedAt: Date?

    /// Future: attach when Files UI is implemented.
    @Published var files: SessionFilesModel?
    /// Future: attach when Tunnels UI is implemented.
    @Published var tunnels: SessionTunnelsModel?

    /// App shell status sink (ErrorNotifier-style).
    var onStatus: ((String, StatusLevel) -> Void)?
    /// Fired once when the session becomes connected (for recent-connection tracking).
    var onConnectedOnce: ((UUID) -> Void)?

    private let controller = ESSSessionController()
    private var shellId: UUID?
    private var didRecordRecent = false

    struct HostKeyPromptData: Identifiable {
        let id = UUID()
        let reason: ESSHostKeyPromptReason
        let fingerprint: String
        let contextLabel: String
    }

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

    private func wireController() {
        controller.onConnected = { [weak self] shellId in
            Task { @MainActor in
                guard let self else { return }
                self.shellId = shellId
                self.state = .connected
                self.connectedAt = Date()
                self.overlayMessage = nil
                self.showReconnect = false
                self.statusMessage = "Connected"
                self.onStatus?("Connected: \(self.title)", .success)
                if !self.didRecordRecent {
                    self.didRecordRecent = true
                    self.onConnectedOnce?(self.connection.connectionId as UUID)
                }
            }
        }
        controller.onData = { [weak self] _, data in
            Task { @MainActor in
                guard let self else { return }
                self.pendingTerminalData = data
                self.terminalFeedToken &+= 1
            }
        }
        controller.onShellClosed = { [weak self] _ in
            Task { @MainActor in
                guard let self else { return }
                self.overlayMessage = "Shell closed. Reconnect to open a new shell."
                self.showReconnect = true
                self.statusMessage = "Shell closed"
                self.connectedAt = nil
                self.onStatus?("Shell closed: \(self.title)", .warning)
            }
        }
        controller.onHostKeyPrompt = { [weak self] reason, fingerprint, context in
            Task { @MainActor in
                guard let self else { return }
                self.hostKeyPrompt = HostKeyPromptData(
                    reason: reason,
                    fingerprint: fingerprint,
                    contextLabel: context
                )
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
        controller.connect(
            withConnection: connection,
            credentials: credentials,
            cols: lastCols,
            rows: lastRows
        )
    }

    func disconnect() {
        controller.disconnect()
    }

    func reconnect() {
        state = .connecting
        connectedAt = nil
        overlayMessage = "Connecting…"
        showReconnect = false
        controller.reconnect(withCols: lastCols, rows: lastRows)
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
        }
    }

    func sendTerminalData(_ data: Data) {
        guard state == .connected else { return }
        controller.write(data, shellId: shellId)
    }

    func resizeTerminal(cols: Int, rows: Int) {
        lastCols = max(cols, 2)
        lastRows = max(rows, 2)
        guard state == .connected else { return }
        controller.resizeCols(Int(lastCols), rows: Int(lastRows), shellId: shellId)
    }

    func pasteClipboard() {
        guard let str = NSPasteboard.general.string(forType: .string), !str.isEmpty,
              let data = str.data(using: .utf8)
        else { return }
        sendTerminalData(data)
    }

    // MARK: - Extension points (ready for Files / Tunnels UI)

    func ensureFilesModel() {
        if files == nil { files = SessionFilesModel() }
    }

    func ensureTunnelsModel() {
        if tunnels == nil { tunnels = SessionTunnelsModel() }
    }

    var sessionController: ESSSessionController { controller }
}
