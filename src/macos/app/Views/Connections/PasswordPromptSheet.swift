// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct PasswordPromptSheet: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Password Required")
                .font(.title2.weight(.semibold))

            if let request = appModel.passwordPrompt {
                let name = request.connection.name.isEmpty
                    ? "\(request.connection.username)@\(request.connection.host)"
                    : request.connection.name
                Text("Enter the password for \(name).")
                    .foregroundStyle(.secondary)
            }

            SecureField("Password", text: $appModel.passwordPromptValue)
                .textFieldStyle(.roundedBorder)

            HStack {
                Spacer()
                Button("Cancel") {
                    appModel.cancelPasswordPrompt()
                }
                .keyboardShortcut(.cancelAction)

                Button("Connect") {
                    appModel.submitPasswordPrompt()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(appModel.passwordPromptValue.isEmpty)
            }
        }
        .padding(20)
        .frame(width: 400)
    }
}
