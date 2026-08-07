// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct SessionsEmptyView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "terminal")
                .font(.system(size: 48))
                .foregroundStyle(.secondary)

            Text("No Sessions")
                .font(.title2.weight(.semibold))

            Text("Connect to a host to open a SwiftTerm shell backed by Qt Core + libssh.")
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .frame(maxWidth: 420)

            HStack(spacing: 12) {
                Button("New Connection…") {
                    appModel.openConnectSheet()
                }
                .keyboardShortcut(.defaultAction)

                Button("Browse Connections…") {
                    appModel.openConnectionManager()
                }
            }

            if !appModel.recentConnections.isEmpty {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Recent")
                        .font(.headline)
                        .foregroundStyle(.secondary)
                    ForEach(appModel.recentConnections, id: \.connectionId) { info in
                        Button {
                            appModel.connect(withId: info.connectionId as UUID)
                        } label: {
                            HStack {
                                Image(systemName: "clock.arrow.circlepath")
                                Text(recentTitle(info))
                                    .lineLimit(1)
                                Spacer()
                                Text("\(info.username)@\(info.host)")
                                    .foregroundStyle(.secondary)
                                    .lineLimit(1)
                            }
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                        .background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 8))
                    }
                }
                .frame(maxWidth: 420)
                .padding(.top, 8)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding(24)
    }

    private func recentTitle(_ info: ESSConnectionInfo) -> String {
        let name = info.name.trimmingCharacters(in: .whitespacesAndNewlines)
        if !name.isEmpty { return name }
        if !info.displayText.isEmpty { return info.displayText }
        return "\(info.username)@\(info.host):\(info.port)"
    }
}
