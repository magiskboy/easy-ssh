// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct HostKeySheet: View {
    let prompt: SessionViewModel.HostKeyPromptData
    let onRespond: (Bool) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text(title)
                .font(.title3.weight(.semibold))

            if !prompt.contextLabel.isEmpty {
                Text(prompt.contextLabel)
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }

            Text(bodyText)
                .fixedSize(horizontal: false, vertical: true)

            Text(prompt.fingerprint)
                .font(.system(.body, design: .monospaced))
                .textSelection(.enabled)
                .padding(8)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(Color.secondary.opacity(0.12), in: RoundedRectangle(cornerRadius: 6))

            HStack {
                Spacer()
                Button("Abort", role: .cancel) {
                    onRespond(false)
                }
                Button(prompt.reason == .unknown ? "Trust & Continue" : "Replace & Continue") {
                    onRespond(true)
                }
                .keyboardShortcut(.defaultAction)
            }
        }
        .padding(20)
        .frame(width: 520)
    }

    private var title: String {
        switch prompt.reason {
        case .unknown: return "Unknown Host Key"
        case .changed: return "Host Key Changed"
        case .other: return "Host Key Warning"
        @unknown default: return "Host Key"
        }
    }

    private var bodyText: String {
        switch prompt.reason {
        case .unknown:
            return "The server host key is not in known_hosts. Trust this fingerprint to continue?"
        case .changed:
            return "WARNING: The host key for this server has changed. This may indicate a man-in-the-middle attack."
        case .other:
            return "WARNING: The host key type differs from known_hosts. Only continue if you trust this key."
        @unknown default:
            return "Review the host key fingerprint before continuing."
        }
    }
}
