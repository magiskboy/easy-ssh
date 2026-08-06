// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct FileExplorerView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        Group {
            if appModel.sessions.isEmpty {
                EmptyStateView(
                    title: "No Session",
                    systemImage: "folder",
                    message: "Connect to a host to browse remote files.",
                    actionTitle: "New Connection",
                    action: { appModel.openConnectSheet() }
                )
            } else if let session = appModel.selectedSession {
                FileExplorerPane(session: session)
                    .id(session.id)
            } else {
                EmptyStateView(
                    title: "Select a Session",
                    systemImage: "rectangle.stack",
                    message: "Choose a connection to browse its remote files."
                )
            }
        }
        .onAppear {
            appModel.selectedSession?.ensureFilesModel()
        }
        .onChange(of: appModel.selectedSessionId) { _, _ in
            appModel.selectedSession?.ensureFilesModel()
        }
    }
}

struct FileExplorerPane: View {
    @EnvironmentObject private var appModel: AppModel
    @ObservedObject var session: SessionViewModel
    @ObservedObject private var files: SessionFilesModel

    init(session: SessionViewModel) {
        self.session = session
        session.ensureFilesModel()
        self._files = ObservedObject(wrappedValue: session.files!)
    }

    var body: some View {
        VStack(spacing: 0) {
            content
            if files.isTransferring || files.transferInterrupted {
                Divider()
                transferStrip
            }
        }
        .alert("New Folder", isPresented: Binding(
            get: { files.mkdirPrompt != nil },
            set: { if !$0 { files.mkdirPrompt = nil } }
        )) {
            TextField("Folder name", text: Binding(
                get: { files.mkdirPrompt ?? "" },
                set: { files.mkdirPrompt = $0 }
            ))
            Button("Create") { files.confirmMkdir() }
            Button("Cancel", role: .cancel) { files.mkdirPrompt = nil }
        } message: {
            Text("Enter a name for the new folder.")
        }
        .alert("Rename", isPresented: Binding(
            get: { files.renamePrompt != nil },
            set: { if !$0 { files.renamePrompt = nil } }
        )) {
            TextField("Name", text: Binding(
                get: { files.renamePrompt?.name ?? "" },
                set: { newValue in
                    if var p = files.renamePrompt {
                        p.name = newValue
                        files.renamePrompt = p
                    }
                }
            ))
            Button("Rename") { files.confirmRename() }
            Button("Cancel", role: .cancel) { files.renamePrompt = nil }
        }
        .alert("Create Symlink", isPresented: Binding(
            get: { files.symlinkPrompt != nil },
            set: { if !$0 { files.symlinkPrompt = nil } }
        )) {
            TextField("Link name", text: Binding(
                get: { files.symlinkPrompt?.linkName ?? "" },
                set: { newValue in
                    if var p = files.symlinkPrompt {
                        p.linkName = newValue
                        files.symlinkPrompt = p
                    }
                }
            ))
            Button("Create") { files.confirmSymlink() }
            Button("Cancel", role: .cancel) { files.symlinkPrompt = nil }
        } message: {
            if let target = files.symlinkPrompt?.targetName {
                Text("Create a symlink to “\(target)”.")
            }
        }
        .alert("Delete?", isPresented: Binding(
            get: { files.deleteConfirmNames != nil },
            set: { if !$0 { files.deleteConfirmNames = nil } }
        )) {
            Button("Delete", role: .destructive) { files.confirmDelete() }
            Button("Cancel", role: .cancel) { files.deleteConfirmNames = nil }
        } message: {
            let names = files.deleteConfirmNames ?? []
            Text("Delete \(names.joined(separator: ", "))? This cannot be undone.")
        }
        .alert("Overwrite Existing Files?", isPresented: Binding(
            get: { files.overwriteConfirm != nil },
            set: { if !$0 { files.overwriteConfirm = nil } }
        )) {
            Button("Overwrite", role: .destructive) { files.confirmOverwrite(overwrite: true) }
            Button("Cancel", role: .cancel) { files.confirmOverwrite(overwrite: false) }
        } message: {
            let names = files.overwriteConfirm?.conflicts ?? []
            Text("Already on the server:\n\(names.joined(separator: "\n"))")
        }
        .focusable()
        .focusEffectDisabled()
        .onKeyPress { press in
            handleFileExplorerShortcut(press)
        }
        .id(appModel.settingsEpoch)
    }

    private func handleFileExplorerShortcut(_ press: KeyPress) -> KeyPress.Result {
        _ = appModel.settingsEpoch
        let settings = ESSAppSettings.shared()
        if KeySequence.matches(press, portable: settings.shortcut(forActionId: "fileExplorer.refresh")) {
            files.refresh()
            return .handled
        }
        if KeySequence.matches(press, portable: settings.shortcut(forActionId: "fileExplorer.upload")) {
            files.uploadFiles()
            return .handled
        }
        if KeySequence.matches(press, portable: settings.shortcut(forActionId: "fileExplorer.download")) {
            files.downloadSelected()
            return .handled
        }
        if KeySequence.matches(press, portable: settings.shortcut(forActionId: "fileExplorer.rename")) {
            files.promptRename()
            return .handled
        }
        if KeySequence.matches(press, portable: settings.shortcut(forActionId: "fileExplorer.delete")) {
            files.promptDelete()
            return .handled
        }
        if KeySequence.matches(press, portable: settings.shortcut(forActionId: "fileExplorer.openWith")) {
            files.openWithSelected()
            return .handled
        }
        return .ignored
    }

    @ViewBuilder
    private var content: some View {
        if session.state != .connected {
            EmptyStateView(
                title: "Not Connected",
                systemImage: "wifi.slash",
                message: "Reconnect the session to browse remote files.",
                actionTitle: session.showReconnect ? "Reconnect" : nil,
                action: session.showReconnect ? { session.reconnect() } : nil
            )
        } else if let reason = files.unavailableReason {
            EmptyStateView(
                title: "Files Unavailable",
                systemImage: "exclamationmark.triangle",
                message: "\(reason)\n\nTerminal session is still active."
            )
        } else if files.isListing && files.entries.isEmpty {
            ProgressView("Loading…")
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if files.cwd.isEmpty {
            EmptyStateView(
                title: "Preparing",
                systemImage: "folder",
                message: "Resolving remote startup directory…"
            )
        } else {
            fileTable
        }
    }

    @ViewBuilder
    private var fileTable: some View {
        switch (files.showSizeColumn, files.showPermissionsColumn, files.showModifiedColumn) {
        case (false, false, false):
            tableBase
        case (true, false, false):
            Table(files.entries, selection: $files.selectedPaths) {
                nameColumn
                sizeColumn
            }
            .contextMenu { selectionMenu }
        case (false, true, false):
            Table(files.entries, selection: $files.selectedPaths) {
                nameColumn
                permissionsColumn
            }
            .contextMenu { selectionMenu }
        case (false, false, true):
            Table(files.entries, selection: $files.selectedPaths) {
                nameColumn
                modifiedColumn
            }
            .contextMenu { selectionMenu }
        case (true, true, false):
            Table(files.entries, selection: $files.selectedPaths) {
                nameColumn
                sizeColumn
                permissionsColumn
            }
            .contextMenu { selectionMenu }
        case (true, false, true):
            Table(files.entries, selection: $files.selectedPaths) {
                nameColumn
                sizeColumn
                modifiedColumn
            }
            .contextMenu { selectionMenu }
        case (false, true, true):
            Table(files.entries, selection: $files.selectedPaths) {
                nameColumn
                permissionsColumn
                modifiedColumn
            }
            .contextMenu { selectionMenu }
        case (true, true, true):
            Table(files.entries, selection: $files.selectedPaths) {
                nameColumn
                sizeColumn
                permissionsColumn
                modifiedColumn
            }
            .contextMenu { selectionMenu }
        }
    }

    private var tableBase: some View {
        Table(files.entries, selection: $files.selectedPaths) {
            nameColumn
        }
        .contextMenu { selectionMenu }
    }

    @TableColumnBuilder<RemoteFileEntry, Never>
    private var nameColumn: some TableColumnContent<RemoteFileEntry, Never> {
        TableColumn("Name") { (entry: RemoteFileEntry) in
            HStack(spacing: 6) {
                Image(systemName: iconName(for: entry))
                    .foregroundStyle(entry.isNavigableDirectory ? Color.accentColor : .secondary)
                    .frame(width: 16)
                Text(entry.name)
                    .lineLimit(1)
                if entry.isSymlink {
                    Text("→ \(entry.linkTarget)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                }
            }
            .contentShape(Rectangle())
            .onTapGesture(count: 2) {
                files.activate(entry)
            }
            .contextMenu { contextMenu(for: entry) }
        }
        .width(min: 180, ideal: 280)
    }

    @TableColumnBuilder<RemoteFileEntry, Never>
    private var sizeColumn: some TableColumnContent<RemoteFileEntry, Never> {
        TableColumn("Size") { (entry: RemoteFileEntry) in
            Text(entry.isNavigableDirectory || entry.isParentNav ? "—" : formatSize(entry.size))
                .foregroundStyle(.secondary)
                .monospacedDigit()
        }
        .width(min: 70, ideal: 90)
    }

    @TableColumnBuilder<RemoteFileEntry, Never>
    private var permissionsColumn: some TableColumnContent<RemoteFileEntry, Never> {
        TableColumn("Permissions") { (entry: RemoteFileEntry) in
            Text(entry.isParentNav ? "" : entry.permissions)
                .foregroundStyle(.secondary)
                .monospaced()
        }
        .width(min: 90, ideal: 110)
    }

    @TableColumnBuilder<RemoteFileEntry, Never>
    private var modifiedColumn: some TableColumnContent<RemoteFileEntry, Never> {
        TableColumn("Modified") { (entry: RemoteFileEntry) in
            Text(entry.isParentNav || entry.mtime <= 0 ? "" : formatMtime(entry.mtime))
                .foregroundStyle(.secondary)
        }
        .width(min: 120, ideal: 150)
    }

    @ViewBuilder
    private var selectionMenu: some View {
        if !files.selectedPaths.isEmpty {
            selectionContextMenu()
        }
    }

    private var transferStrip: some View {
        HStack(spacing: 12) {
            if files.transferInterrupted {
                Image(systemName: "exclamationmark.triangle.fill")
                    .foregroundStyle(.orange)
                Text(files.interruptedJob?.summary ?? "Transfer interrupted")
                    .lineLimit(1)
                Spacer()
                Button("Resume") { files.resumeTransfer() }
                Button("Discard", role: .destructive) { files.discardTransfer() }
            } else {
                if let fraction = files.transferFraction {
                    ProgressView(value: fraction)
                        .frame(maxWidth: 180)
                } else {
                    ProgressView()
                        .controlSize(.small)
                }
                Text(files.transferCurrentName.isEmpty ? "Transferring…" : files.transferCurrentName)
                    .lineLimit(1)
                if files.transferBytesTotal > 0 {
                    Text("\(files.transferBytesDone) / \(files.transferBytesTotal)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .monospacedDigit()
                }
                Spacer()
                Button("Cancel") { files.cancelTransfer() }
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(.bar)
    }

    @ViewBuilder
    private func contextMenu(for entry: RemoteFileEntry) -> some View {
        if entry.isParentNav {
            Button("Open") { files.activate(entry) }
        } else {
            Button("Open") {
                files.selectedPaths = [entry.path]
                files.activate(entry)
            }
            Button("Download") {
                files.selectedPaths = [entry.path]
                files.downloadSelected()
            }
            Button("Open With…") {
                files.selectedPaths = [entry.path]
                files.openWithSelected()
            }
            .disabled(entry.isNavigableDirectory)
            Divider()
            Button("Upload Files…") { files.uploadFiles() }
            Button("Upload Folder…") { files.uploadFolder() }
            Divider()
            Button("New Folder…") { files.promptMkdir() }
            Button("Create Symlink…") {
                files.selectedPaths = [entry.path]
                files.promptSymlink()
            }
            Button("Rename…") {
                files.selectedPaths = [entry.path]
                files.promptRename()
            }
            Button("Copy Path") {
                files.selectedPaths = [entry.path]
                files.copySelectedPath()
            }
            Divider()
            Button("Delete…", role: .destructive) {
                files.selectedPaths = [entry.path]
                files.promptDelete()
            }
            Divider()
            Button("Refresh") { files.refresh() }
        }
    }

    @ViewBuilder
    private func selectionContextMenu() -> some View {
        Button("Open") {
            if let entry = files.selectedEntries.first {
                files.activate(entry)
            }
        }
        Button("Download") { files.downloadSelected() }
            .disabled(files.selectedDownloadablePaths.isEmpty)
        Button("Open With…") { files.openWithSelected() }
            .disabled(files.selectedEntries.allSatisfy { $0.isNavigableDirectory || $0.isParentNav })
        Divider()
        Button("Upload Files…") { files.uploadFiles() }
        Button("Upload Folder…") { files.uploadFolder() }
        Divider()
        Button("New Folder…") { files.promptMkdir() }
        Button("Create Symlink…") { files.promptSymlink() }
            .disabled(files.selectedEntries.filter { !$0.isParentNav }.count != 1)
        Button("Rename…") { files.promptRename() }
            .disabled(files.selectedEntries.filter { !$0.isParentNav }.count != 1)
        Button("Copy Path") { files.copySelectedPath() }
        Divider()
        Button("Delete…", role: .destructive) { files.promptDelete() }
            .disabled(files.selectedEntries.filter { !$0.isParentNav }.isEmpty)
        Divider()
        Button("Refresh") { files.refresh() }
    }

    private func iconName(for entry: RemoteFileEntry) -> String {
        if entry.isParentNav { return "arrow.up.left" }
        if entry.isSymlink { return entry.linkIsDir ? "folder.fill.badge.gearshape" : "link" }
        if entry.isDir { return "folder.fill" }
        return "doc"
    }

    private func formatSize(_ size: Int64) -> String {
        let formatter = ByteCountFormatter()
        formatter.countStyle = .file
        return formatter.string(fromByteCount: size)
    }

    private func formatMtime(_ mtime: TimeInterval) -> String {
        let date = Date(timeIntervalSince1970: mtime)
        return date.formatted(date: .abbreviated, time: .shortened)
    }
}
