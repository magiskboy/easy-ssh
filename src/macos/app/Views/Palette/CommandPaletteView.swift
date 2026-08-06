// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct CommandPaletteView: View {
    @EnvironmentObject private var appModel: AppModel
    let mode: PaletteMode

    @State private var filter = ""
    @State private var selectedIndex = 0
    @FocusState private var filterFocused: Bool

    private var rows: [PaletteRow] {
        appModel.paletteRows(mode: mode, query: filter)
    }

    private var selectableIndices: [Int] {
        rows.enumerated().compactMap { index, row in
            row.isSelectable ? index : nil
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(mode.title)
                .font(.headline)

            TextField(mode.filterPlaceholder, text: $filter)
                .textFieldStyle(.roundedBorder)
                .focused($filterFocused)
                .onSubmit { activateSelection() }

            List {
                ForEach(Array(rows.enumerated()), id: \.element.id) { index, row in
                    paletteRowView(row)
                        .listRowBackground(index == selectedIndex ? Color.accentColor.opacity(0.15) : nil)
                        .contentShape(Rectangle())
                        .onTapGesture {
                            if row.isSelectable {
                                selectedIndex = index
                                activateSelection()
                            }
                        }
                }
            }
            .frame(minHeight: 220, maxHeight: 320)
            .onChange(of: filter) { _, _ in
                selectedIndex = selectableIndices.first ?? 0
            }

            Text("↑↓ navigate · Enter select · Esc close")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .padding(16)
        .frame(width: 520)
        .onAppear {
            filter = ""
            selectedIndex = selectableIndices.first ?? 0
            filterFocused = true
        }
        .onExitCommand {
            appModel.dismissCommandPalette()
        }
        .onKeyPress(.upArrow) {
            moveSelection(delta: -1)
            return .handled
        }
        .onKeyPress(.downArrow) {
            moveSelection(delta: 1)
            return .handled
        }
        .onKeyPress(.return) {
            activateSelection()
            return .handled
        }
    }

    @ViewBuilder
    private func paletteRowView(_ row: PaletteRow) -> some View {
        switch row {
        case let .action(item, _):
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text(item.label)
                        .foregroundStyle(item.enabled ? .primary : .secondary)
                    Text(item.group)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                if !item.shortcutText.isEmpty {
                    Text(item.shortcutText)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        case let .connection(item, _):
            VStack(alignment: .leading, spacing: 2) {
                Text(item.name)
                if !item.subtitle.isEmpty {
                    Text(item.subtitle)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        case .createConnection:
            Label("Create connection…", systemImage: "plus.circle")
        case let .shell(item, _):
            VStack(alignment: .leading, spacing: 2) {
                Text(item.shellTitle)
                Text(item.subtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        case let .hint(text):
            Text(text)
                .foregroundStyle(.secondary)
                .font(.callout)
        }
    }

    private func moveSelection(delta: Int) {
        let indices = selectableIndices
        guard !indices.isEmpty else { return }
        guard let pos = indices.firstIndex(of: selectedIndex) else {
            selectedIndex = indices[0]
            return
        }
        let next = (pos + delta + indices.count) % indices.count
        selectedIndex = indices[next]
    }

    private func activateSelection() {
        guard selectedIndex >= 0, selectedIndex < rows.count else { return }
        let row = rows[selectedIndex]
        guard row.isSelectable else { return }
        appModel.activatePaletteRow(row, filter: filter)
    }
}
