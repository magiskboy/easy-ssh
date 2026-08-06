// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

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
}

@MainActor
final class AppModel: ObservableObject {
    @Published var sidebarMode: SidebarMode = .sessions
    @Published var sessions: [SessionViewModel] = []
    @Published var selectedSessionId: UUID?
    @Published var showConnectSheet: Bool = false
    @Published var connectDraft = ConnectionDraft()

    var selectedSession: SessionViewModel? {
        sessions.first { $0.id == selectedSessionId }
    }

    func openConnectSheet() {
        connectDraft = ConnectionDraft()
        showConnectSheet = true
    }

    func connect(with draft: ConnectionDraft) {
        let session = SessionViewModel(draft: draft)
        sessions.append(session)
        selectedSessionId = session.id
        showConnectSheet = false
        sidebarMode = .sessions
        session.connect()
    }

    func closeSession(_ id: UUID) {
        if let idx = sessions.firstIndex(where: { $0.id == id }) {
            sessions[idx].disconnect()
            sessions.remove(at: idx)
        }
        if selectedSessionId == id {
            selectedSessionId = sessions.last?.id
        }
    }

    func pasteClipboardIntoActiveSession() {
        selectedSession?.pasteClipboard()
    }
}
