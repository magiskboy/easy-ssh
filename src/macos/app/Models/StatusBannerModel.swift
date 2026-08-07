// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

enum StatusLevel: Equatable {
    case status
    case success
    case warning
    case error

    func foregroundColor(isDark: Bool) -> Color {
        switch self {
        case .status:
            return Color.primary
        case .success:
            return isDark
                ? Color(red: 0.506, green: 0.780, blue: 0.518)
                : Color(red: 0.180, green: 0.490, blue: 0.196)
        case .warning:
            return isDark
                ? Color(red: 1.0, green: 0.718, blue: 0.302)
                : Color(red: 0.937, green: 0.424, blue: 0.0)
        case .error:
            return isDark
                ? Color(red: 0.937, green: 0.325, blue: 0.314)
                : Color(red: 0.776, green: 0.157, blue: 0.157)
        }
    }
}

struct StatusAlert: Identifiable, Equatable {
    let id = UUID()
    let title: String
    let message: String
    let level: StatusLevel
}

/// Mirrors Qt ErrorNotifier: status bar updates + optional modal alert.
@MainActor
final class StatusBannerModel: ObservableObject {
    @Published var message: String = "Ready"
    @Published var level: StatusLevel = .status
    @Published var alert: StatusAlert?

    func post(_ text: String, level: StatusLevel = .status) {
        message = text
        self.level = level
    }

    func notify(title: String, message: String, level: StatusLevel = .warning) {
        post(message, level: level)
        alert = StatusAlert(title: title, message: message, level: level)
    }

    func dismissAlert() {
        alert = nil
    }
}
