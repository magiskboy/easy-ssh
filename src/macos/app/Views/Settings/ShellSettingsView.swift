// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

struct ShellSettingsView: View {
    @EnvironmentObject private var settingsModel: SettingsModel

    var body: some View {
        Form {
            Section("Font") {
                TextField("Monospace font family", text: $settingsModel.terminalFontFamily)
                HStack {
                    Text("Size")
                    TextField("pt", value: $settingsModel.terminalFontPointSize, format: .number)
                        .frame(width: 64)
                    Text("pt").foregroundStyle(.secondary)
                }
            }

            Section("Colors") {
                Picker("Color scheme", selection: $settingsModel.colorScheme) {
                    if !AppAppearance.terminalColorSchemes.contains(settingsModel.colorScheme) {
                        Text(settingsModel.colorScheme).tag(settingsModel.colorScheme)
                    }
                    ForEach(AppAppearance.terminalColorSchemes, id: \.self) { name in
                        Text(name).tag(name)
                    }
                }
                Text("SwiftTerm uses simplified fg/bg mapping for terminal schemes.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section("Cursor") {
                Picker("Shape", selection: $settingsModel.cursorShape) {
                    Text("Block").tag(0)
                    Text("Underline").tag(1)
                    Text("I-Beam").tag(2)
                }
                Toggle("Blink cursor", isOn: $settingsModel.cursorBlink)
            }

            Section("Scrollback") {
                Stepper(value: $settingsModel.historySize, in: -1 ... 1_000_000, step: 500) {
                    if settingsModel.historySize < 0 {
                        Text("Scrollback: Unlimited")
                    } else {
                        Text("Scrollback: \(settingsModel.historySize) lines")
                    }
                }
            }

            Section("Input") {
                Toggle("Warn before multiline paste", isOn: $settingsModel.confirmMultilinePaste)
            }

            Section("Layout") {
                Toggle("Smart layout for new shells", isOn: $settingsModel.smartLayout)
            }
        }
        .formStyle(.grouped)
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
    }
}
