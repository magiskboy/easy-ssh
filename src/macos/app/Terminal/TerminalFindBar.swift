// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct TerminalFindBar: View {
    @ObservedObject var shell: ShellViewModel
    var onClose: () -> Void

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "magnifyingglass")
                .foregroundStyle(.secondary)
            TextField("Find", text: $shell.findQuery)
                .textFieldStyle(.roundedBorder)
                .frame(maxWidth: 240)
                .onSubmit { shell.findNext() }
            Text(matchLabel)
                .font(.caption.monospacedDigit())
                .foregroundStyle(.secondary)
                .frame(minWidth: 40, alignment: .trailing)
            Button {
                shell.findPrevious()
            } label: {
                Image(systemName: "chevron.up")
            }
            .disabled(shell.findQuery.isEmpty)
            .help("Find Previous")
            Button {
                shell.findNext()
            } label: {
                Image(systemName: "chevron.down")
            }
            .disabled(shell.findQuery.isEmpty)
            .help("Find Next")
            Button {
                shell.clearFind()
                onClose()
            } label: {
                Image(systemName: "xmark")
            }
            .help("Close Find")
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(.bar)
    }

    private var matchLabel: String {
        if shell.findQuery.isEmpty {
            return ""
        }
        if shell.findMatchTotal == 0 {
            return "0/0"
        }
        return "\(shell.findMatchIndex)/\(shell.findMatchTotal)"
    }
}
