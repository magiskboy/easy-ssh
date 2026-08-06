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
                    appModel.openShellInSelectedSession()
                } label: {
                    Image(systemName: "plus.rectangle.on.rectangle")
                }
                .help("New Shell")
                .disabled(!(appModel.selectedSession?.canOpenShell ?? false))

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
        VStack(spacing: 0) {
            if !session.shells.isEmpty {
                shellStrip
                Divider()
            }

            ZStack {
                if let tree = session.layout, !session.shells.isEmpty {
                    ShellSplitView(session: session, node: tree)
                        .opacity(session.state == .connected && session.overlayMessage == nil ? 1 : 0.35)
                } else if session.state == .connected {
                    EmptyStateView(
                        title: "No Shell",
                        systemImage: "terminal",
                        message: "Open a new shell from the Session menu."
                    )
                } else {
                    Color.clear
                }

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
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .sheet(item: $session.renameRequest) { request in
            RenameShellSheet(
                title: Binding(
                    get: { session.renameRequest?.title ?? request.title },
                    set: { newValue in
                        if var r = session.renameRequest {
                            r.title = newValue
                            session.renameRequest = r
                        }
                    }
                ),
                onCancel: { session.renameRequest = nil },
                onSave: {
                    if let r = session.renameRequest {
                        session.commitRename(request: r)
                    }
                }
            )
        }
        .alert(
            "Paste Multiple Lines?",
            isPresented: Binding(
                get: { session.pendingMultilinePaste != nil },
                set: { if !$0 { session.cancelMultilinePaste() } }
            )
        ) {
            Button("Paste", role: .none) {
                session.confirmMultilinePaste()
            }
            Button("Cancel", role: .cancel) {
                session.cancelMultilinePaste()
            }
        } message: {
            let lines = session.pendingMultilinePaste?
                .split(whereSeparator: \.isNewline).count ?? 0
            Text("Clipboard contains \(lines) lines. Paste into the terminal?")
        }
    }

    private var shellStrip: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 4) {
                ForEach(session.shells) { shell in
                    ShellTabChip(
                        title: shell.title,
                        isSelected: shell.id == session.focusedShellId,
                        isInLayout: session.layout?.contains(shell.id) ?? false
                    ) {
                        session.focusShell(shell.id)
                    } onClose: {
                        session.closeShell(shell.id)
                    } onRename: {
                        session.beginRenameShell(shell.id)
                    }
                }
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
        }
        .background(.bar)
    }
}

private struct ShellTabChip: View {
    let title: String
    let isSelected: Bool
    let isInLayout: Bool
    let onSelect: () -> Void
    let onClose: () -> Void
    let onRename: () -> Void

    var body: some View {
        HStack(spacing: 6) {
            if !isInLayout {
                Image(systemName: "rectangle.on.rectangle")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .help("Not in current split — click to show")
            }
            Text(title)
                .lineLimit(1)
            Button(action: onClose) {
                Image(systemName: "xmark")
                    .font(.caption2.weight(.bold))
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
        .background(isSelected ? Color.accentColor.opacity(0.25) : Color.secondary.opacity(0.12))
        .clipShape(RoundedRectangle(cornerRadius: 5))
        .onTapGesture(perform: onSelect)
        .contextMenu {
            Button("Rename…", action: onRename)
            Button("Close", role: .destructive, action: onClose)
        }
    }
}

struct ShellSplitView: View {
    @ObservedObject var session: SessionViewModel
    let node: ShellLayoutNode

    var body: some View {
        switch node {
        case let .leaf(id):
            if let shell = session.shells.first(where: { $0.id == id }) {
                ShellPaneView(session: session, shell: shell)
            } else {
                Color.black.opacity(0.05)
            }
        case let .split(axis, first, second):
            switch axis {
            case .horizontal:
                HSplitView {
                    ShellSplitView(session: session, node: first)
                    ShellSplitView(session: session, node: second)
                }
            case .vertical:
                VSplitView {
                    ShellSplitView(session: session, node: first)
                    ShellSplitView(session: session, node: second)
                }
            }
        }
    }
}

struct ShellPaneView: View {
    @ObservedObject var session: SessionViewModel
    @ObservedObject var shell: ShellViewModel

    var body: some View {
        VStack(spacing: 0) {
            if session.showFindBar && session.focusedShellId == shell.id {
                TerminalFindBar(shell: shell) {
                    session.showFindBar = false
                    shell.showFindBar = false
                    shell.clearFind()
                }
            }
            TerminalRepresentable(session: session, shell: shell)
                .overlay(
                    RoundedRectangle(cornerRadius: 0)
                        .strokeBorder(
                            session.focusedShellId == shell.id
                                ? Color.accentColor.opacity(0.7)
                                : Color.clear,
                            lineWidth: 2
                        )
                )
                .onTapGesture {
                    session.focusShell(shell.id)
                }
        }
    }
}

private struct RenameShellSheet: View {
    @Binding var title: String
    let onCancel: () -> Void
    let onSave: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Rename Shell")
                .font(.headline)
            TextField("Title", text: $title)
                .textFieldStyle(.roundedBorder)
            HStack {
                Spacer()
                Button("Cancel", action: onCancel)
                    .keyboardShortcut(.cancelAction)
                Button("Save") {
                    onSave()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
        .padding(20)
        .frame(width: 360)
    }
}
