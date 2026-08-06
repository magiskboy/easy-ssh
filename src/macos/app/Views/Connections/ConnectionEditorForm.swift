// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct ConnectionEditorForm: View {
    @Binding var form: ConnectionFormState
    var isReadOnly: Bool = false

    var body: some View {
        Form {
            Section("Session") {
                TextField("Name", text: $form.name)
                    .disabled(isReadOnly)
                TextField("Host", text: $form.host)
                    .disabled(isReadOnly)
                TextField("Port", value: $form.port, format: .number)
                    .disabled(isReadOnly)
                TextField("Username", text: $form.username)
                    .disabled(isReadOnly)

                Picker("Authentication", selection: $form.usePrivateKey) {
                    Text("Password").tag(false)
                    Text("Private Key").tag(true)
                }
                .disabled(isReadOnly)

                if form.usePrivateKey {
                    HStack {
                        TextField("Private key path", text: $form.privateKeyPath)
                            .disabled(isReadOnly)
                        if !isReadOnly {
                            Button("Browse…") { pickPrivateKey() }
                        }
                    }
                    SecureField("Passphrase", text: $form.passphrase)
                        .disabled(isReadOnly)
                } else {
                    SecureField("Password", text: $form.password)
                        .disabled(isReadOnly)
                    Toggle("Save password in Keychain", isOn: $form.savePassword)
                        .disabled(isReadOnly)
                }

                if form.source == .sshConfig, !form.configAlias.isEmpty {
                    LabeledContent("SSH Config Alias", value: form.configAlias)
                }
            }
        }
        .formStyle(.grouped)
    }

    private func pickPrivateKey() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.message = "Select an OpenSSH private key"
        if panel.runModal() == .OK, let url = panel.url {
            form.privateKeyPath = url.path
            form.usePrivateKey = true
        }
    }
}
