// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

enum PaletteMode: String, Equatable, Identifiable {
    case actions
    case connections
    case terminals

    var id: String { rawValue }

    var title: String {
        switch self {
        case .actions: return "Command Palette"
        case .connections: return "Quick Connect"
        case .terminals: return "Go to Terminal"
        }
    }

    var filterPlaceholder: String {
        switch self {
        case .actions: return "Filter actions…"
        case .connections: return "Search connections or create…"
        case .terminals: return "Search open terminals…"
        }
    }
}

struct PaletteActionItem: Identifiable, Equatable {
    let actionId: String
    let label: String
    let group: String
    let shortcutText: String
    let enabled: Bool

    var id: String { actionId }
}

struct PaletteConnectionItem: Identifiable, Equatable {
    let connectionId: UUID
    let name: String
    let subtitle: String
    let searchFields: [String]
    let recentRank: Int

    var id: UUID { connectionId }
}

struct PaletteTerminalItem: Identifiable, Equatable {
    let connectionId: UUID
    let terminalId: UUID
    let sessionTitle: String
    let terminalTitle: String
    let subtitle: String
    let searchFields: [String]
    let isActive: Bool

    var id: String { "\(connectionId.uuidString)-\(terminalId.uuidString)" }
}

enum PaletteRow: Identifiable, Equatable {
    case action(PaletteActionItem, score: Int)
    case connection(PaletteConnectionItem, score: Int)
    case createConnection(score: Int)
    case terminal(PaletteTerminalItem, score: Int)
    case hint(String)

    var id: String {
        switch self {
        case let .action(item, _): return "action-\(item.actionId)"
        case let .connection(item, _): return "conn-\(item.connectionId.uuidString)"
        case .createConnection: return "create"
        case let .terminal(item, _): return "terminal-\(item.id)"
        case let .hint(text): return "hint-\(text)"
        }
    }

    var score: Int {
        switch self {
        case let .action(_, score): return score
        case let .connection(_, score): return score
        case .createConnection(let score): return score
        case let .terminal(_, score): return score
        case .hint: return Int.min
        }
    }

    var isSelectable: Bool {
        switch self {
        case .hint: return false
        case let .action(item, _): return item.enabled
        default: return true
        }
    }
}

/// Action ids that open palette modes — excluded from the runnable action list.
enum PaletteMetaActions {
    static let excluded: Set<String> = [
        "general.commandPalette",
        "general.quickConnect",
        "session.goToTerminal",
    ]
}
