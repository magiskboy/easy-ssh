// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct TerminalFindBar: View {
    @ObservedObject var terminal: TerminalViewModel
    var onClose: () -> Void

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "magnifyingglass")
                .foregroundStyle(.secondary)
            TextField("Find", text: $terminal.findQuery)
                .textFieldStyle(.roundedBorder)
                .frame(maxWidth: 240)
                .onSubmit { terminal.findNext() }
            Text(matchLabel)
                .font(.caption.monospacedDigit())
                .foregroundStyle(.secondary)
                .frame(minWidth: 40, alignment: .trailing)
            Button {
                terminal.findPrevious()
            } label: {
                Image(systemName: "chevron.up")
            }
            .disabled(terminal.findQuery.isEmpty)
            .help("Find Previous")
            Button {
                terminal.findNext()
            } label: {
                Image(systemName: "chevron.down")
            }
            .disabled(terminal.findQuery.isEmpty)
            .help("Find Next")
            Button {
                terminal.clearFind()
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
        if terminal.findQuery.isEmpty {
            return ""
        }
        if terminal.findMatchTotal == 0 {
            return "0/0"
        }
        return "\(terminal.findMatchIndex)/\(terminal.findMatchTotal)"
    }
}
