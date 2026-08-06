// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct ContentView: View {
    @Environment(\.openWindow) private var openWindow
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        // Prefer VStack over safeAreaInset — the latter overlays AppKit columns on macOS.
        VStack(spacing: 0) {
            NavigationSplitView {
                ConnectionSidebarView()
                    .environmentObject(appModel)
                    .navigationSplitViewColumnWidth(min: 220, ideal: 260, max: 320)
            } content: {
                SessionWorkspaceColumn()
                    .environmentObject(appModel)
                    .navigationSplitViewColumnWidth(min: 500, ideal: 760)
            } detail: {
                FileInspectorColumn()
                    .environmentObject(appModel)
                    .navigationSplitViewColumnWidth(min: 260, ideal: 340, max: 460)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            Divider()
            AppStatusBar()
        }
        .background(WindowFrameTracker())
        .background(AppLifecycleBridge())
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button {
                    appModel.openShellInSelectedSession()
                } label: {
                    Image(systemName: "plus.rectangle.on.rectangle")
                }
                .help("New Terminal")
                .disabled(!(appModel.selectedSession?.canOpenShell ?? false))

                Button {
                    appModel.presentExplorer(.container)
                } label: {
                    Image(systemName: "shippingbox")
                }
                .help("Open Container Explorer")
                .disabled(appModel.selectedSession?.state != .connected)

                Button {
                    appModel.presentExplorer(.service)
                } label: {
                    Image(systemName: "switch.2")
                }
                .help("Open Service Explorer")
                .disabled(appModel.selectedSession?.state != .connected)

                Button {
                    appModel.presentExplorer(.process)
                } label: {
                    Image(systemName: "list.bullet.rectangle.portrait")
                }
                .help("Open Process Explorer")
                .disabled(appModel.selectedSession?.state != .connected)

                Button {
                    appModel.presentExplorer(.systemInfo)
                } label: {
                    Image(systemName: "info.circle")
                }
                .help("Open System Info")
                .disabled(appModel.selectedSession?.state != .connected)
            }
        }
        .preferredColorScheme(appAppearanceColorScheme)
        .modifier(AppUIFontModifier(epoch: appModel.settingsEpoch))
        // Single sheet host — stacked .sheet modifiers silently fail on macOS.
        .sheet(item: $appModel.activeModal) { modal in
            switch modal {
            case .connect:
                ConnectSheet()
                    .environmentObject(appModel)
            case .passwordPrompt:
                PasswordPromptSheet()
                    .environmentObject(appModel)
            case .hostKeyPrompt:
                if let prompt = appModel.activeHostKeyPrompt {
                    HostKeySheet(prompt: prompt) { accept in
                        appModel.respondHostKey(accept: accept)
                    }
                } else {
                    Color.clear
                        .onAppear {
                            appModel.activeModal = nil
                        }
                }
            case .about:
                AboutView()
            }
        }
        .alert(item: Binding(
            get: { appModel.status.alert },
            set: { appModel.status.alert = $0 }
        )) { alert in
            Alert(
                title: Text(alert.title),
                message: Text(alert.message),
                dismissButton: .default(Text("OK")) {
                    appModel.status.dismissAlert()
                }
            )
        }
        .sheet(item: $appModel.paletteMode) { mode in
            CommandPaletteView(mode: mode)
                .environmentObject(appModel)
        }
        .onChange(of: appModel.explorerWindowOpenToken) { _, token in
            guard token != nil else { return }
            openWindow(id: "explorer")
        }
        .onChange(of: appModel.connectionManagerOpenToken) { _, token in
            guard token != nil else { return }
            openWindow(id: "connectionManager")
        }
    }

    private var appAppearanceColorScheme: ColorScheme? {
        _ = appModel.settingsEpoch
        let themeId = ESSAppSettings.shared().themeId
        return AppAppearance.preferredColorScheme(themeId: themeId)
    }
}

private struct ConnectionSidebarView: View {
    @EnvironmentObject private var appModel: AppModel
    @State private var pendingDeleteId: UUID?
    @State private var showDeleteConfirm = false

    var body: some View {
        List(selection: sidebarSelection) {
            if appModel.library.allConnections.isEmpty {
                ContentUnavailableView("No Connections", systemImage: "server.rack")
            } else {
                ForEach(appModel.library.allConnections, id: \.connectionId) { info in
                    ConnectionSidebarRow(info: info)
                        .tag(info.connectionId as UUID)
                        .contextMenu {
                            connectionContextMenu(for: info)
                        }
                }
            }
        }
        .listStyle(.sidebar)
        .navigationTitle("Connections")
        .safeAreaInset(edge: .bottom) {
            Button {
                appModel.openConnectSheet()
            } label: {
                Label("New Connection", systemImage: "plus.circle.fill")
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .buttonStyle(.borderless)
            .padding(12)
        }
        .alert("Delete Connection?", isPresented: $showDeleteConfirm) {
            Button("Cancel", role: .cancel) {
                pendingDeleteId = nil
            }
            Button("Delete", role: .destructive) {
                if let id = pendingDeleteId {
                    _ = appModel.deleteConnection(id)
                    pendingDeleteId = nil
                }
            }
        } message: {
            Text("This removes the saved connection and its Keychain secrets. Open sessions are not closed.")
        }
    }

    @ViewBuilder
    private func connectionContextMenu(for info: ESSConnectionInfo) -> some View {
        let connectionId = info.connectionId as UUID
        let session = appModel.session(forConnectionId: connectionId)
        let isActive = session?.state == .connected || session?.state == .connecting

        if isActive {
            Button("Disconnect") {
                appModel.disconnectConnection(connectionId)
            }
        } else {
            Button("Connect") {
                appModel.connectOrReconnect(connectionId: connectionId)
            }
        }

        Button("Edit") {
            appModel.editConnection(connectionId)
        }

        if info.source == .app {
            Button("Duplicate") {
                _ = appModel.duplicateConnection(connectionId)
            }
            Divider()
            Button("Delete", role: .destructive) {
                pendingDeleteId = connectionId
                showDeleteConfirm = true
            }
        } else {
            Button("Import to Easy SSH…") {
                if let imported = appModel.library.importFromSshConfig(id: connectionId) {
                    appModel.status.post(
                        "Imported: \(imported.name.isEmpty ? imported.displayText : imported.name)",
                        level: .status
                    )
                }
            }
        }
    }

    private var sidebarSelection: Binding<UUID?> {
        Binding(
            get: { appModel.selectedConnectionId },
            set: { newValue in
                guard let newValue else { return }
                appModel.selectConnection(newValue)
            }
        )
    }
}

private struct ConnectionSidebarRow: View {
    @EnvironmentObject private var appModel: AppModel
    let info: ESSConnectionInfo

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(statusColor)
                .frame(width: 8, height: 8)
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .lineLimit(1)
                Text(subtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }
            Spacer(minLength: 0)
            if info.source == .sshConfig {
                Text("Config")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 2)
    }

    private var title: String {
        let name = info.name.trimmingCharacters(in: .whitespacesAndNewlines)
        if !name.isEmpty { return name }
        if !info.displayText.isEmpty { return info.displayText }
        return "\(info.username)@\(info.host)"
    }

    private var subtitle: String {
        "\(info.username)@\(info.host):\(info.port)"
    }

    private var statusColor: Color {
        guard let session = appModel.session(forConnectionId: info.connectionId as UUID) else {
            return .secondary.opacity(0.45)
        }
        switch session.state {
        case .connected: return .green
        case .connecting: return .orange
        case .failed: return .red
        case .disconnected, .idle: return .secondary
        }
    }
}

private struct SessionWorkspaceColumn: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        Group {
            if let session = appModel.selectedSession {
                SessionPane(session: session)
                    .id(session.id)
            } else if let connection = appModel.selectedConnection {
                EmptyStateView(
                    title: connectionTitle(connection),
                    systemImage: "terminal",
                    message: "Opening or focusing the session for this connection."
                )
            } else if appModel.library.allConnections.isEmpty {
                EmptyStateView(
                    title: "No Connections",
                    systemImage: "server.rack",
                    message: "Create or import a connection to start a session.",
                    actionTitle: "New Connection",
                    action: { appModel.openConnectSheet() }
                )
            } else {
                EmptyStateView(
                    title: "Select a Connection",
                    systemImage: "rectangle.stack",
                    message: "Choose a connection in the sidebar to open its session workspace."
                )
            }
        }
        .navigationTitle(appModel.selectedSession?.title ?? "Workspace")
    }

    private func connectionTitle(_ info: ESSConnectionInfo) -> String {
        let name = info.name.trimmingCharacters(in: .whitespacesAndNewlines)
        if !name.isEmpty { return name }
        if !info.displayText.isEmpty { return info.displayText }
        return "\(info.username)@\(info.host)"
    }
}

private struct FileInspectorColumn: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        Group {
            if let session = appModel.selectedSession {
                FileExplorerPane(session: session)
                    .id(session.id)
            } else if let connection = appModel.selectedConnection {
                EmptyStateView(
                    title: "File Explorer",
                    systemImage: "folder",
                    message: "Connect to \(connectionTitle(connection)) to browse remote files."
                )
            } else {
                EmptyStateView(
                    title: "File Explorer",
                    systemImage: "folder",
                    message: "Select a connection to load remote files here."
                )
            }
        }
        .navigationTitle("Files")
    }

    private func connectionTitle(_ info: ESSConnectionInfo) -> String {
        let name = info.name.trimmingCharacters(in: .whitespacesAndNewlines)
        if !name.isEmpty { return name }
        if !info.displayText.isEmpty { return info.displayText }
        return "\(info.username)@\(info.host)"
    }
}

private struct AppUIFontModifier: ViewModifier {
    let epoch: Int

    private var resolvedFont: Font? {
        let _ = epoch
        let settings = ESSAppSettings.shared()
        return AppAppearance.uiFont(
            family: settings.uiFontFamily,
            pointSize: settings.uiFontPointSize,
            mode: settings.uiFontMode
        )
    }

    func body(content: Content) -> some View {
        Group {
            if let resolvedFont {
                content.font(resolvedFont)
            } else {
                content
            }
        }
    }
}
