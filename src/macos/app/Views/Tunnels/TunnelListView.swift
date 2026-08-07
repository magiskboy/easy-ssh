// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct TunnelListView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        VStack(spacing: 0) {
            if appModel.sessions.isEmpty {
                EmptyStateView(
                    title: "No Session",
                    systemImage: "network",
                    message: "Connect to a host to manage tunnels.",
                    actionTitle: "New Connection",
                    action: { appModel.openConnectSheet() }
                )
            } else {
                sessionTabs
                Divider()
                if let session = appModel.selectedSession {
                    TunnelListPane(session: session)
                        .id(session.id)
                } else {
                    EmptyStateView(
                        title: "Select a Session",
                        systemImage: "rectangle.stack",
                        message: "Choose a session tab to manage tunnels."
                    )
                }
            }
        }
        .onAppear {
            appModel.selectedSession?.ensureTunnelsModel()
        }
        .onChange(of: appModel.selectedSessionId) { _, _ in
            appModel.selectedSession?.ensureTunnelsModel()
        }
        .onChange(of: appModel.sidebarMode) { _, mode in
            if mode == .tunnels {
                appModel.selectedSession?.ensureTunnelsModel()
            }
        }
    }

    private var sessionTabs: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 4) {
                ForEach(appModel.sessions) { session in
                    TunnelSessionTabChip(
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

private struct TunnelSessionTabChip: View {
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

struct TunnelListPane: View {
    @ObservedObject var session: SessionViewModel
    @ObservedObject private var tunnels: SessionTunnelsModel

    init(session: SessionViewModel) {
        self.session = session
        session.ensureTunnelsModel()
        self._tunnels = ObservedObject(wrappedValue: session.tunnels!)
    }

    var body: some View {
        Group {
            if session.state != .connected {
                EmptyStateView(
                    title: "Session Not Connected",
                    systemImage: "network",
                    message: "Connect the session to manage tunnels."
                )
            } else if tunnels.rows.isEmpty {
                EmptyStateView(
                    title: "No Tunnels",
                    systemImage: "point.3.connected.trianglepath.dotted",
                    message: "No tunnels for this connection.",
                    actionTitle: "Add Tunnel",
                    action: { tunnels.beginCreate() }
                )
            } else {
                Table(tunnels.rows, selection: $tunnels.selectedTunnelId) {
                    TableColumn("Name") { row in
                        VStack(alignment: .leading, spacing: 2) {
                            Text(row.name.isEmpty ? "Untitled" : row.name)
                            if !row.statusDetail.isEmpty {
                                Text(row.statusDetail)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                    .lineLimit(1)
                            }
                        }
                    }
                    TableColumn("Type") { row in
                        Text(row.typeTitle)
                    }
                    TableColumn("Local") { row in
                        Text(row.localAddress)
                            .font(.system(.body, design: .monospaced))
                    }
                    TableColumn("Remote") { row in
                        Text(row.remoteAddress)
                            .font(.system(.body, design: .monospaced))
                    }
                    TableColumn("Status") { row in
                        Text(row.status)
                            .foregroundStyle(statusColor(row.status))
                    }
                }
                .contextMenu {
                    Button("Add Tunnel") { tunnels.beginCreate() }
                    Button("Edit Tunnel") { tunnels.beginEditSelection() }
                        .disabled(!tunnels.canEditSelection)
                    Button(tunnels.selectedRow?.definition.enabled == true ? "Disable" : "Enable") {
                        tunnels.toggleSelectionEnabled()
                    }
                    .disabled(tunnels.selectedRow == nil || session.state != .connected)
                    Divider()
                    Button("Delete Tunnel", role: .destructive) {
                        tunnels.requestDeleteSelection()
                    }
                    .disabled(tunnels.selectedRow == nil)
                }
            }
        }
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button {
                    tunnels.beginCreate()
                } label: {
                    Label("Add", systemImage: "plus")
                }

                Button {
                    tunnels.beginEditSelection()
                } label: {
                    Label("Edit", systemImage: "pencil")
                }
                .disabled(!tunnels.canEditSelection)

                Button(role: .destructive) {
                    tunnels.requestDeleteSelection()
                } label: {
                    Label("Delete", systemImage: "trash")
                }
                .disabled(tunnels.selectedRow == nil)

                Button {
                    tunnels.toggleSelectionEnabled()
                } label: {
                    Label(tunnels.selectedRow?.definition.enabled == true ? "Disable" : "Enable",
                          systemImage: tunnels.selectedRow?.definition.enabled == true ? "pause.circle" : "play.circle")
                }
                .disabled(tunnels.selectedRow == nil || session.state != .connected)

                Button {
                    tunnels.stopAll()
                } label: {
                    Label("Stop All", systemImage: "stop.circle")
                }
                .disabled(session.state != .connected || tunnels.rows.isEmpty)
            }
        }
        .sheet(item: $tunnels.editor) { editor in
            TunnelEditorSheet(tunnels: tunnels, session: session, context: editor)
        }
        .alert(item: $tunnels.pendingDelete) { definition in
            Alert(
                title: Text("Delete Tunnel?"),
                message: Text("Delete tunnel \"\(definition.name)\"?"),
                primaryButton: .destructive(Text("Delete")) {
                    tunnels.deletePending()
                },
                secondaryButton: .cancel {
                    tunnels.pendingDelete = nil
                }
            )
        }
        .onTapGesture {
            if tunnels.selectedTunnelId == nil, let first = tunnels.rows.first {
                tunnels.selectedTunnelId = first.id
            }
        }
    }

    private func statusColor(_ status: String) -> Color {
        switch status {
        case "Listening": return .green
        case "Starting": return .orange
        case "Error": return .red
        default: return .secondary
        }
    }
}
