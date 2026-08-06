// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

enum AppAppearance {
    static let appearanceThemeIds: [(label: String, id: String)] = [
        ("System", "system"),
        ("Light", "modern_light"),
        ("Dark", "modern_dark"),
    ]

    static func normalizedThemeId(_ raw: String) -> String {
        let id = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if appearanceThemeIds.contains(where: { $0.id == id }) {
            return id
        }
        return "system"
    }

    static func preferredColorScheme(themeId: String) -> ColorScheme? {
        switch normalizedThemeId(themeId) {
        case "modern_light": return .light
        case "modern_dark": return .dark
        default: return nil
        }
    }

    static func uiFont(family: String, pointSize: Double, mode: String) -> Font? {
        guard mode == "custom" else { return nil }
        let size = pointSize > 0 ? pointSize : 13
        if family.isEmpty {
            return .system(size: size)
        }
        return .custom(family, size: size)
    }

    /// QTermWidget scheme names (subset bundled with Easy SSH).
    static let terminalColorSchemes: [String] = [
        "WhiteOnBlack",
        "BlackOnWhite",
        "Linux",
        "GreenOnBlack",
        "Solarized",
        "SolarizedLight",
        "Nord",
        "Tango",
        "DarkPastels",
        "Ubuntu",
        "Falcon",
        "BreezeModified",
    ].sorted { $0.localizedCaseInsensitiveCompare($1) == .orderedAscending }
}
