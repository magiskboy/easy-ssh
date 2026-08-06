// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct AboutView: View {
    @Environment(\.dismiss) private var dismiss

    private var version: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "—"
    }

    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "terminal.fill")
                .font(.system(size: 48))
                .foregroundStyle(Color.accentColor)

            Text("Easy SSH")
                .font(.title.weight(.semibold))

            Text(
                "A lightweight SSH / SFTP client.\n\n"
                    + "Version: \(version)\n"
                    + "UI: SwiftUI + SwiftTerm\n"
                    + "Core: Qt Core + libssh"
            )
            .multilineTextAlignment(.center)
            .foregroundStyle(.secondary)

            Link(
                "github.com/magiskboy/easy-ssh",
                destination: URL(string: "https://github.com/magiskboy/easy-ssh")!
            )

            Button("Close") {
                dismiss()
            }
            .keyboardShortcut(.defaultAction)
            .padding(.top, 8)
        }
        .padding(28)
        .frame(width: 360)
    }
}
