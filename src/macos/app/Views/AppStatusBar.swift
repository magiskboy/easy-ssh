// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct AppStatusBar: View {
    @EnvironmentObject private var appModel: AppModel
    @Environment(\.colorScheme) private var colorScheme

    @State private var now = Date()

    private let tick = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    var body: some View {
        HStack(spacing: 12) {
            Text(appModel.status.message)
                .font(.caption)
                .foregroundStyle(appModel.status.level.foregroundColor(isDark: colorScheme == .dark))
                .lineLimit(1)
                .truncationMode(.tail)

            Spacer(minLength: 8)

            if let info = sessionInfoText {
                Text(info)
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .frame(maxWidth: .infinity)
        .background(.bar)
        .onReceive(tick) { date in
            if appModel.selectedSession?.state == .connected {
                now = date
            }
        }
    }

    private var sessionInfoText: String? {
        guard let session = appModel.selectedSession else { return nil }
        let host = session.connection.host.isEmpty ? "—" : session.connection.host
        let user = session.connection.username.isEmpty ? "—" : session.connection.username
        let shell = "main"
        let ttl: String
        if session.state == .connected, let connectedAt = session.connectedAt {
            ttl = Self.formatTtl(seconds: max(0, Int(now.timeIntervalSince(connectedAt))))
        } else {
            ttl = "—"
        }
        return "\(host) | \(user) | \(shell) | \(ttl)"
    }

    static func formatTtl(seconds: Int) -> String {
        let hours = seconds / 3600
        let minutes = (seconds % 3600) / 60
        let secs = seconds % 60
        return String(format: "%02d:%02d:%02d", hours, minutes, secs)
    }
}
