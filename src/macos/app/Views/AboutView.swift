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
        VStack(spacing: 10) {
            Image(systemName: "terminal.fill")
                .font(.system(size: 36))
                .foregroundStyle(Color.accentColor)

            Text("Easy SSH")
                .font(.title2.weight(.semibold))

            Text("A lightweight SSH / SFTP client.")
                .font(.callout)
                .foregroundStyle(.secondary)

            VStack(spacing: 2) {
                Text("Version: \(version)")
                Text("UI: SwiftUI + SwiftTerm")
                Text("Core: Qt Core + libssh")
            }
            .font(.caption)
            .foregroundStyle(.secondary)
            .multilineTextAlignment(.center)
            .padding(.top, 2)

            Link(
                "github.com/magiskboy/easy-ssh",
                destination: URL(string: "https://github.com/magiskboy/easy-ssh")!
            )
            .font(.caption)

            Button("Close") {
                dismiss()
            }
            .keyboardShortcut(.defaultAction)
            .padding(.top, 4)
        }
        .padding(.horizontal, 24)
        .padding(.vertical, 20)
        .frame(width: 300)
        .fixedSize()
        .modifier(FittedSheetSizingModifier())
    }
}

/// Prefer content-fitted sheet sizing when the OS supports it (macOS 15+).
private struct FittedSheetSizingModifier: ViewModifier {
    func body(content: Content) -> some View {
        if #available(macOS 15.0, *) {
            content.presentationSizing(.fitted)
        } else {
            content
        }
    }
}
