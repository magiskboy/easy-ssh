// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct ConnectionManagerView: View {
    @EnvironmentObject private var appModel: AppModel
    @Environment(\.dismiss) private var dismiss

    @State private var searchText = ""
    @State private var sourceFilter: ConnectionSourceFilter = .all
    @State private var selectedId: UUID?
    @State private var editorForm = ConnectionFormState()
    @State private var baselineForm = ConnectionFormState()
    @State private var isCreating = false
    @State private var showDeleteConfirm = false
    @State private var pendingDeleteId: UUID?

    private var library: ConnectionLibrary { appModel.library }

    private var filtered: [ESSConnectionInfo] {
        library.filtered(query: searchText, source: sourceFilter)
    }

    private var isDirty: Bool {
        isCreating || editorForm != baselineForm
    }

    private var selectedInfo: ESSConnectionInfo? {
        guard let selectedId else { return nil }
        return library.connection(id: selectedId)
    }

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            HSplitView {
                connectionList
                    .frame(minWidth: 240, idealWidth: 280, maxWidth: 360)
                editorPane
                    .frame(minWidth: 360)
            }
            Divider()
            footer
        }
        .frame(width: 900, height: 640)
        .onAppear {
            library.reload()
            if let first = filtered.first {
                select(first)
            }
        }
        .alert("Delete Connection?", isPresented: $showDeleteConfirm) {
            Button("Cancel", role: .cancel) {
                pendingDeleteId = nil
            }
            Button("Delete", role: .destructive) {
                if let id = pendingDeleteId {
                    _ = library.delete(id: id)
                    if selectedId == id {
                        selectedId = nil
                        isCreating = false
                        editorForm = ConnectionFormState()
                        baselineForm = editorForm
                    }
                    pendingDeleteId = nil
                }
            }
        } message: {
            Text("This removes the saved connection and its Keychain secrets. Open sessions are not closed.")
        }
    }

    private var header: some View {
        HStack(spacing: 12) {
            Text("Connection Manager")
                .font(.title2.weight(.semibold))
            Spacer()
            TextField("Search", text: $searchText)
                .textFieldStyle(.roundedBorder)
                .frame(width: 180)
            Picker("Source", selection: $sourceFilter) {
                ForEach(ConnectionSourceFilter.allCases) { filter in
                    Text(filter.title).tag(filter)
                }
            }
            .pickerStyle(.segmented)
            .frame(width: 260)
            Button("New") { beginCreate() }
            Button("Import…") { importSelectedOrPick() }
            Button("Reload SSH Config") { library.reloadSshConfig() }
        }
        .padding(12)
    }

    private var connectionList: some View {
        List(selection: Binding(
            get: { selectedId },
            set: { newId in
                guard newId != selectedId else { return }
                if isDirty {
                    // Discard create/edit draft when switching without save.
                    isCreating = false
                }
                if let newId, let info = library.connection(id: newId) {
                    select(info)
                } else {
                    selectedId = nil
                }
            }
        )) {
            ForEach(filtered, id: \.connectionId) { info in
                ConnectionRow(info: info)
                    .tag(info.connectionId as UUID)
                    .contextMenu {
                        Button("Open Session") {
                            openSelected(info)
                        }
                        if info.source == .app {
                            Button("Duplicate") {
                                if let copy = library.duplicate(id: info.connectionId as UUID) {
                                    select(copy)
                                }
                            }
                            Button("Delete…", role: .destructive) {
                                pendingDeleteId = info.connectionId as UUID
                                showDeleteConfirm = true
                            }
                        } else {
                            Button("Import to Easy SSH…") {
                                if let imported = library.importFromSshConfig(id: info.connectionId as UUID) {
                                    select(imported)
                                }
                            }
                        }
                    }
            }
        }
        .listStyle(.sidebar)
    }

    private var editorPane: some View {
        VStack(alignment: .leading, spacing: 0) {
            if selectedId != nil || isCreating {
                ConnectionEditorForm(form: $editorForm, isReadOnly: editorForm.isReadOnly && !isCreating)
                Divider()
                HStack {
                    if editorForm.source == .sshConfig, !isCreating {
                        Text("SSH Config hosts are read-only. Import to edit.")
                            .foregroundStyle(.secondary)
                            .font(.caption)
                    } else if let error = editorForm.validationError(isCreate: isCreating),
                              isDirty
                    {
                        Text(error)
                            .foregroundStyle(.red)
                            .font(.caption)
                            .lineLimit(2)
                    }
                    Spacer()
                    if isDirty && (isCreating || editorForm.source == .app) {
                        Button("Discard") { discardEdits() }
                        Button("Save") { saveEdits() }
                            .keyboardShortcut(.defaultAction)
                            .disabled(!editorForm.isValid(isCreate: isCreating))
                    }
                }
                .padding(12)
            } else {
                EmptyStateView(
                    title: "No Connection Selected",
                    systemImage: "server.rack",
                    message: "Select a connection or create a new one.",
                    actionTitle: "New Connection",
                    action: { beginCreate() }
                )
            }
        }
    }

    private var footer: some View {
        HStack {
            Button("Open Session") {
                if let info = selectedInfo {
                    openSelected(info)
                }
            }
            .disabled(selectedInfo == nil || isCreating)
            .keyboardShortcut(.defaultAction)

            if let info = selectedInfo, info.source == .app, !isCreating {
                Button("Duplicate") {
                    if let copy = library.duplicate(id: info.connectionId as UUID) {
                        select(copy)
                    }
                }
                Button("Delete…", role: .destructive) {
                    pendingDeleteId = info.connectionId as UUID
                    showDeleteConfirm = true
                }
            }

            if let info = selectedInfo, info.source == .sshConfig, !isCreating {
                Button("Import to Easy SSH…") {
                    if let imported = library.importFromSshConfig(id: info.connectionId as UUID) {
                        select(imported)
                    }
                }
            }

            Spacer()
            Button("Close") { dismiss() }
                .keyboardShortcut(.cancelAction)
        }
        .padding(12)
    }

    // MARK: - Actions

    private func select(_ info: ESSConnectionInfo) {
        selectedId = info.connectionId as UUID
        isCreating = false
        editorForm = ConnectionFormState.from(info)
        baselineForm = editorForm
    }

    private func beginCreate() {
        isCreating = true
        selectedId = nil
        var form = ConnectionFormState()
        form.connectionId = UUID()
        editorForm = form
        baselineForm = ConnectionFormState() // force dirty
        baselineForm.connectionId = UUID() // different so isDirty
    }

    private func discardEdits() {
        if isCreating {
            isCreating = false
            selectedId = nil
            editorForm = ConnectionFormState()
            baselineForm = editorForm
        } else if let info = selectedInfo {
            select(info)
        }
    }

    private func saveEdits() {
        if let error = editorForm.validationError(isCreate: isCreating) {
            appModel.status.notify(title: "Validation", message: error, level: .warning)
            return
        }

        if isCreating {
            let info = editorForm.makeConnectionInfo()
            guard library.add(info) else {
                appModel.status.notify(
                    title: "Save Connection",
                    message: "Could not save the connection.",
                    level: .error
                )
                return
            }
            ConnectionSecretHelper.persistSecrets(
                for: info,
                previousAuthType: nil,
                isEdit: false,
                password: editorForm.usePrivateKey ? nil : editorForm.password,
                passphrase: editorForm.usePrivateKey ? editorForm.passphrase : nil,
                gatewayPassword: editorForm.gatewayPassword.isEmpty ? nil : editorForm.gatewayPassword,
                gatewayPassphrase: editorForm.gatewayPassphrase.isEmpty
                    ? nil : editorForm.gatewayPassphrase
            )
            select(info)
            return
        }

        guard let existing = selectedInfo, existing.source == .app else { return }
        let previousAuth = existing.authType
        let info = editorForm.makeConnectionInfo()
        guard library.update(info) else {
            appModel.status.notify(
                title: "Save Connection",
                message: "Could not update the connection.",
                level: .error
            )
            return
        }
        ConnectionSecretHelper.persistSecrets(
            for: info,
            previousAuthType: previousAuth,
            isEdit: true,
            password: editorForm.usePrivateKey
                ? nil
                : (editorForm.password.isEmpty ? nil : editorForm.password),
            passphrase: editorForm.usePrivateKey
                ? (editorForm.passphrase.isEmpty ? nil : editorForm.passphrase)
                : nil,
            gatewayPassword: editorForm.gatewayPassword.isEmpty ? nil : editorForm.gatewayPassword,
            gatewayPassphrase: editorForm.gatewayPassphrase.isEmpty
                ? nil : editorForm.gatewayPassphrase
        )
        select(info)
    }

    private func openSelected(_ info: ESSConnectionInfo) {
        if isCreating {
            appModel.status.notify(
                title: "Unsaved Connection",
                message: "Save the new connection before opening a session.",
                level: .warning
            )
            return
        }
        if isDirty && info.source == .app {
            appModel.status.notify(
                title: "Unsaved Changes",
                message: "Save or discard changes before opening a session.",
                level: .warning
            )
            return
        }
        if let error = ConnectionFormState.from(info).validationError(isCreate: false) {
            appModel.status.notify(title: "Validation", message: error, level: .warning)
            return
        }
        appModel.connect(with: info, inlineCredentials: nil)
    }

    private func importSelectedOrPick() {
        if let info = selectedInfo, info.source == .sshConfig {
            if let imported = library.importFromSshConfig(id: info.connectionId as UUID) {
                select(imported)
            }
            return
        }
        library.reloadSshConfig()
        appModel.status.post("Reloaded ~/.ssh/config — select a host and Import.", level: .status)
    }
}

private struct ConnectionRow: View {
    let info: ESSConnectionInfo

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack {
                Text(rowTitle)
                    .lineLimit(1)
                Spacer()
                Text(info.source == .app ? "App" : "Config")
                    .font(.caption2)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(info.source == .app ? Color.accentColor.opacity(0.15) : Color.secondary.opacity(0.15))
                    .clipShape(RoundedRectangle(cornerRadius: 4))
            }
            Text(subtitle)
                .font(.caption)
                .foregroundStyle(.secondary)
                .lineLimit(1)
        }
        .padding(.vertical, 2)
    }

    private var rowTitle: String {
        let name = info.name.trimmingCharacters(in: .whitespacesAndNewlines)
        if !name.isEmpty { return name }
        if !info.displayText.isEmpty { return info.displayText }
        return "\(info.username)@\(info.host)"
    }

    private var subtitle: String {
        "\(info.username)@\(info.host):\(info.port)"
    }
}
