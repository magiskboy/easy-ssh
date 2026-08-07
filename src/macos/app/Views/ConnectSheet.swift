// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct ConnectSheet: View {
    @EnvironmentObject private var appModel: AppModel
    @Environment(\.dismiss) private var dismiss

    private let labelWidth: CGFloat = 108

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("Quick Connect")
                .font(.headline)

            VStack(alignment: .leading, spacing: 10) {
                fieldRow("Host") {
                    TextField("host or user@host", text: $appModel.connectDraft.host)
                        .textFieldStyle(.roundedBorder)
                        .onSubmit(applyHostQueryIfNeeded)
                }

                HStack(alignment: .center, spacing: 12) {
                    fieldRow("Port") {
                        TextField("22", value: $appModel.connectDraft.port, format: .number)
                            .textFieldStyle(.roundedBorder)
                            .frame(width: 72)
                    }
                    fieldRow("Username") {
                        TextField("user", text: $appModel.connectDraft.username)
                            .textFieldStyle(.roundedBorder)
                    }
                }

                Divider().padding(.vertical, 2)

                toggleRow("Private key", isOn: $appModel.connectDraft.usePrivateKey)

                if appModel.connectDraft.usePrivateKey {
                    fieldRow("Key path") {
                        HStack(spacing: 8) {
                            TextField("~/.ssh/id_ed25519", text: $appModel.connectDraft.privateKeyPath)
                                .textFieldStyle(.roundedBorder)
                            Button("Browse…") {
                                pickPrivateKey()
                            }
                        }
                    }
                    fieldRow("Passphrase") {
                        SecureField("optional", text: $appModel.connectDraft.passphrase)
                            .textFieldStyle(.roundedBorder)
                    }
                } else {
                    fieldRow("Password") {
                        SecureField("optional", text: $appModel.connectDraft.password)
                            .textFieldStyle(.roundedBorder)
                    }
                }

                Divider().padding(.vertical, 2)

                toggleRow("Save connection", isOn: $appModel.connectDraft.saveConnection)

                if appModel.connectDraft.saveConnection {
                    fieldRow("Name") {
                        TextField("Display name", text: $appModel.connectDraft.name)
                            .textFieldStyle(.roundedBorder)
                            .onAppear {
                                if appModel.connectDraft.name.isEmpty {
                                    appModel.connectDraft.name = appModel.connectDraft.displayName
                                }
                            }
                    }
                    if !appModel.connectDraft.usePrivateKey {
                        toggleRow("Save password", isOn: $appModel.connectDraft.savePassword)
                    }
                }
            }

            HStack(spacing: 8) {
                Spacer(minLength: 0)
                Button("Cancel") {
                    dismiss()
                }
                .keyboardShortcut(.cancelAction)

                Button("Connect") {
                    if appModel.connectDraft.saveConnection,
                       appModel.connectDraft.name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                    {
                        appModel.connectDraft.name = appModel.connectDraft.displayName
                    }
                    appModel.connect(with: appModel.connectDraft)
                }
                .keyboardShortcut(.defaultAction)
                .buttonStyle(.borderedProminent)
                .disabled(!appModel.connectDraft.isValid)
            }
            .padding(.top, 2)
        }
        .padding(20)
        .frame(width: 440)
        .fixedSize(horizontal: true, vertical: true)
        .modifier(FittedSheetSizingModifier())
    }

    @ViewBuilder
    private func fieldRow<Content: View>(_ title: String, @ViewBuilder content: () -> Content) -> some View {
        HStack(alignment: .center, spacing: 10) {
            Text(title)
                .foregroundStyle(.secondary)
                .frame(width: labelWidth, alignment: .trailing)
            content()
        }
    }

    private func toggleRow(_ title: String, isOn: Binding<Bool>) -> some View {
        HStack(alignment: .center, spacing: 10) {
            Text(title)
                .foregroundStyle(.secondary)
                .frame(width: labelWidth, alignment: .trailing)
            Toggle("", isOn: isOn)
                .labelsHidden()
                .toggleStyle(.switch)
            Spacer(minLength: 0)
        }
    }

    private func applyHostQueryIfNeeded() {
        let host = appModel.connectDraft.host
        if host.contains("@") || host.contains(":") {
            let parsed = ConnectionDraft.parseQuery(host)
            var draft = appModel.connectDraft
            if !parsed.username.isEmpty { draft.username = parsed.username }
            draft.host = parsed.host
            draft.port = parsed.port
            appModel.connectDraft = draft
        }
    }

    private func pickPrivateKey() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.message = "Select an OpenSSH private key"
        if panel.runModal() == .OK, let url = panel.url {
            appModel.connectDraft.privateKeyPath = url.path
            appModel.connectDraft.usePrivateKey = true
        }
    }
}

/// Hug content height on macOS 15+; no-op on 14 (deployment target).
private struct FittedSheetSizingModifier: ViewModifier {
    func body(content: Content) -> some View {
        if #available(macOS 15.0, *) {
            content.presentationSizing(.fitted)
        } else {
            content
        }
    }
}
