// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct ConnectSheet: View {
    @EnvironmentObject private var appModel: AppModel
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Quick Connect")
                .font(.title2.weight(.semibold))

            Form {
                TextField("Host", text: $appModel.connectDraft.host)
                TextField("Port", value: $appModel.connectDraft.port, format: .number)
                TextField("Username", text: $appModel.connectDraft.username)

                Toggle("Use private key", isOn: $appModel.connectDraft.usePrivateKey)

                if appModel.connectDraft.usePrivateKey {
                    HStack {
                        TextField("Private key path", text: $appModel.connectDraft.privateKeyPath)
                        Button("Browse…") {
                            pickPrivateKey()
                        }
                    }
                } else {
                    SecureField("Password", text: $appModel.connectDraft.password)
                }
            }
            .formStyle(.grouped)

            HStack {
                Spacer()
                Button("Cancel") {
                    dismiss()
                }
                .keyboardShortcut(.cancelAction)

                Button("Connect") {
                    appModel.connect(with: appModel.connectDraft)
                }
                .keyboardShortcut(.defaultAction)
                .disabled(!appModel.connectDraft.isValid)
            }
        }
        .padding(20)
        .frame(width: 480)
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
