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

@MainActor
final class AppModel: ObservableObject {
    let status = StatusBannerModel()

    @Published var sidebarMode: SidebarMode = .sessions
    @Published var sessions: [SessionViewModel] = []
    @Published var selectedSessionId: UUID?
    @Published var showConnectSheet: Bool = false
    @Published var showAbout: Bool = false
    @Published var connectDraft = ConnectionDraft()

    private var cancellables = Set<AnyCancellable>()

    init() {
        status.objectWillChange
            .sink { [weak self] _ in
                self?.objectWillChange.send()
            }
            .store(in: &cancellables)
    }

    var selectedSession: SessionViewModel? {
        sessions.first { $0.id == selectedSessionId }
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

    func connect(with draft: ConnectionDraft) {
        let session = SessionViewModel(draft: draft)
        wireSession(session)
        sessions.append(session)
        selectedSessionId = session.id
        showConnectSheet = false
        sidebarMode = .sessions
        status.post("Connecting: \(draft.displayName)…", level: .status)
        session.connect()
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

    private func wireSession(_ session: SessionViewModel) {
        session.onStatus = { [weak self] message, level in
            self?.status.post(message, level: level)
        }
    }
}
