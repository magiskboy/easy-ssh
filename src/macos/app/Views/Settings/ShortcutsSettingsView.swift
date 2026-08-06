// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct ShortcutsSettingsView: View {
    @EnvironmentObject private var settingsModel: SettingsModel
    @State private var recordingActionId: String?

    private var groupedActionIds: [(group: String, ids: [String])] {
        let ids = ESSAppSettings.shortcutActionIds().map { $0 as String }
        var order: [String] = []
        var map: [String: [String]] = [:]
        for id in ids {
            let group = ESSAppSettings.shortcutGroup(forActionId: id)
            if map[group] == nil {
                order.append(group)
                map[group] = []
            }
            map[group, default: []].append(id)
        }
        return order.map { ($0, map[$0] ?? []) }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Click a shortcut field, then press the new key combination.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Spacer()
                Button("Reset Defaults") {
                    settingsModel.resetShortcutsInDraftToDefaults()
                }
            }
            .padding(.horizontal)

            List {
                ForEach(groupedActionIds, id: \.group) { section in
                    Section(section.group) {
                        ForEach(section.ids, id: \.self) { actionId in
                            HStack {
                                Text(ESSAppSettings.shortcutLabel(forActionId: actionId))
                                Spacer()
                                ShortcutRecorderField(
                                    portable: binding(for: actionId),
                                    isRecording: recordingActionId == actionId,
                                    onBegin: { recordingActionId = actionId },
                                    onEnd: { recordingActionId = nil }
                                )
                                .frame(width: 160)
                            }
                        }
                    }
                }
            }
        }
    }

    private func binding(for actionId: String) -> Binding<String> {
        Binding(
            get: { settingsModel.shortcutDraft[actionId] ?? "" },
            set: { settingsModel.shortcutDraft[actionId] = $0 }
        )
    }
}

private struct ShortcutRecorderField: View {
    @Binding var portable: String
    let isRecording: Bool
    let onBegin: () -> Void
    let onEnd: () -> Void

    @FocusState private var focused: Bool

    var body: some View {
        HStack {
            Text(displayText)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 6)
                .padding(.vertical, 4)
                .background(Color(nsColor: .textBackgroundColor))
                .clipShape(RoundedRectangle(cornerRadius: 6))
        }
        .contentShape(Rectangle())
        .focusable()
        .focused($focused)
        .onTapGesture {
            onBegin()
            focused = true
        }
        .onKeyPress { press in
            if press.key == KeyEquivalent("\u{1B}") {
                onEnd()
                focused = false
                return .handled
            }
            if let captured = KeySequence.capturePortable(from: press) {
                portable = captured
                onEnd()
                focused = false
                return .handled
            }
            return .ignored
        }
        .overlay {
            if isRecording {
                RoundedRectangle(cornerRadius: 6)
                    .stroke(Color.accentColor, lineWidth: 2)
            }
        }
    }

    private var displayText: String {
        if isRecording { return "Press keys…" }
        let text = KeySequence.displayString(fromPortable: portable.isEmpty ? nil : portable)
        return text.isEmpty ? "—" : text
    }
}
