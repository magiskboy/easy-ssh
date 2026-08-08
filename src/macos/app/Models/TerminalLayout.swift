// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

enum SplitAxis: String, Equatable, Codable {
    /// Side-by-side (`HSplitView`) — Qt Right dock area.
    case horizontal
    /// Stacked (`VSplitView`) — Qt Bottom dock area.
    case vertical
}

/// In-memory split tree for terminals in one session (Phase 4 MVP; not persisted).
indirect enum TerminalLayoutNode: Equatable, Codable {
    enum CodingKeys: String, CodingKey {
        case leaf
        case split
        case axis
        case first
        case second
        case id
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        if let id = try container.decodeIfPresent(UUID.self, forKey: .id) {
            self = .leaf(id)
            return
        }
        let axis = try container.decode(SplitAxis.self, forKey: .axis)
        let first = try container.decode(TerminalLayoutNode.self, forKey: .first)
        let second = try container.decode(TerminalLayoutNode.self, forKey: .second)
        self = .split(axis: axis, first: first, second: second)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        switch self {
        case let .leaf(id):
            try container.encode(id, forKey: .id)
        case let .split(axis, first, second):
            try container.encode(axis, forKey: .axis)
            try container.encode(first, forKey: .first)
            try container.encode(second, forKey: .second)
        }
    }
    case leaf(UUID)
    case split(axis: SplitAxis, first: TerminalLayoutNode, second: TerminalLayoutNode)

    var leafCount: Int {
        switch self {
        case .leaf:
            return 1
        case let .split(_, first, second):
            return first.leafCount + second.leafCount
        }
    }

    var leafIds: [UUID] {
        switch self {
        case let .leaf(id):
            return [id]
        case let .split(_, first, second):
            return first.leafIds + second.leafIds
        }
    }

    func contains(_ id: UUID) -> Bool {
        leafIds.contains(id)
    }

    /// Split `relativeTo` leaf, placing `newId` as the second child (Right/Bottom).
    func splitting(relativeTo: UUID, newId: UUID, axis: SplitAxis) -> TerminalLayoutNode {
        switch self {
        case let .leaf(id):
            if id == relativeTo {
                return .split(axis: axis, first: .leaf(id), second: .leaf(newId))
            }
            return self
        case let .split(existingAxis, first, second):
            if first.contains(relativeTo) {
                return .split(
                    axis: existingAxis,
                    first: first.splitting(relativeTo: relativeTo, newId: newId, axis: axis),
                    second: second
                )
            }
            if second.contains(relativeTo) {
                return .split(
                    axis: existingAxis,
                    first: first,
                    second: second.splitting(relativeTo: relativeTo, newId: newId, axis: axis)
                )
            }
            return self
        }
    }

    /// Replace a leaf UUID (focus swap when layout is full or smart layout is off).
    func replacing(_ oldId: UUID, with newId: UUID) -> TerminalLayoutNode {
        switch self {
        case let .leaf(id):
            return .leaf(id == oldId ? newId : id)
        case let .split(axis, first, second):
            return .split(
                axis: axis,
                first: first.replacing(oldId, with: newId),
                second: second.replacing(oldId, with: newId)
            )
        }
    }

    /// Remove a leaf and collapse single-child splits.
    func removing(_ id: UUID) -> TerminalLayoutNode? {
        switch self {
        case let .leaf(leafId):
            return leafId == id ? nil : self
        case let .split(axis, first, second):
            let left = first.removing(id)
            let right = second.removing(id)
            switch (left, right) {
            case (nil, nil):
                return nil
            case let (nil, r?):
                return r
            case let (l?, nil):
                return l
            case let (l?, r?):
                return .split(axis: axis, first: l, second: r)
            }
        }
    }
}

enum TerminalLayoutCodec {
    static func encode(_ node: TerminalLayoutNode?) -> Data? {
        guard let node else { return nil }
        return try? JSONEncoder().encode(node)
    }

    static func decode(_ data: Data) -> TerminalLayoutNode? {
        try? JSONDecoder().decode(TerminalLayoutNode.self, from: data)
    }
}

enum TerminalLayoutPlanner {
    static let maxVisibleLeaves = 4

    /// Mirror Qt `TerminalLayoutPlanner::AlternateFocus`: odd docked count → Right (H), even → Bottom (V).
    static func axisForNewTerminal(currentLeafCount: Int) -> SplitAxis {
        (currentLeafCount % 2 == 1) ? .horizontal : .vertical
    }
}
