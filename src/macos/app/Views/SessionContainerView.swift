// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct SessionContainerView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        VStack(spacing: 0) {
            if appModel.sessions.isEmpty {
                SessionsEmptyView()
            } else {
                sessionTabs
                Divider()
                if let session = appModel.selectedSession {
                    SessionPane(session: session)
                        .id(session.id)
                } else {
                    EmptyStateView(
                        title: "Select a Session",
                        systemImage: "rectangle.stack",
                        message: "Choose a session tab to focus its terminal."
                    )
                }
            }
        }
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button {
                    appModel.openConnectionManager()
                } label: {
                    Image(systemName: "list.bullet.rectangle")
                }
                .help("Browse Connections")

                Button {
                    appModel.openConnectSheet()
                } label: {
                    Image(systemName: "plus")
                }
                .help("New Connection")
            }
        }
    }

    private var sessionTabs: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 4) {
                ForEach(appModel.sessions) { session in
                    SessionTabChip(
                        title: session.title,
                        isSelected: session.id == appModel.selectedSessionId,
                        state: session.state
                    ) {
                        appModel.selectedSessionId = session.id
                    } onClose: {
                        appModel.closeSession(session.id)
                    }
                }
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 6)
        }
        .background(.bar)
    }
}

private struct SessionTabChip: View {
    let title: String
    let isSelected: Bool
    let state: SessionUIState
    let onSelect: () -> Void
    let onClose: () -> Void

    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(stateColor)
                .frame(width: 7, height: 7)
            Text(title)
                .lineLimit(1)
            Button(action: onClose) {
                Image(systemName: "xmark")
                    .font(.caption2.weight(.bold))
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(isSelected ? Color.accentColor.opacity(0.2) : Color.clear)
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .onTapGesture(perform: onSelect)
    }

    private var stateColor: Color {
        switch state {
        case .connected: return .green
        case .connecting: return .orange
        case .failed: return .red
        case .disconnected, .idle: return .secondary
        }
    }
}

struct SessionPane: View {
    @ObservedObject var session: SessionViewModel

    var body: some View {
        ZStack {
            TerminalRepresentable(session: session)
                .opacity(session.state == .connected && session.overlayMessage == nil ? 1 : 0.35)

            if let overlay = session.overlayMessage {
                VStack(spacing: 12) {
                    Text(overlay)
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.primary)
                        .padding(.horizontal)
                    if session.showReconnect {
                        Button("Reconnect") {
                            session.reconnect()
                        }
                        .keyboardShortcut(.defaultAction)
                    }
                }
                .padding(24)
                .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
            }
        }
        .sheet(item: $session.hostKeyPrompt) { prompt in
            HostKeySheet(prompt: prompt) { accept in
                session.respondHostKey(accept: accept)
            }
        }
    }
}
