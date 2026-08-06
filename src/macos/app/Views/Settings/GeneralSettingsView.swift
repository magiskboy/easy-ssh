// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct GeneralSettingsView: View {
    @EnvironmentObject private var settingsModel: SettingsModel

    var body: some View {
        Form {
            Section("Appearance") {
                Picker("Font", selection: $settingsModel.uiFontMode) {
                    Text("System").tag("system")
                    Text("Custom…").tag("custom")
                }

                if settingsModel.uiFontMode == "custom" {
                    TextField("Font family", text: $settingsModel.uiFontFamily)
                    HStack {
                        Text("Size")
                        TextField("pt", value: $settingsModel.uiFontPointSize, format: .number)
                            .frame(width: 64)
                        Text("pt")
                            .foregroundStyle(.secondary)
                    }
                }

                Picker("Appearance", selection: $settingsModel.themeId) {
                    ForEach(AppAppearance.appearanceThemeIds, id: \.id) { item in
                        Text(item.label).tag(item.id)
                    }
                }
            }

            Section("Session") {
                Toggle("Auto reconnect when connection is lost", isOn: $settingsModel.autoReconnect)
                Toggle("Restore previous workspace on launch", isOn: $settingsModel.restoreWorkspace)
            }

            Section("Window") {
                Toggle("Close window to menu bar (Phase 8)", isOn: $settingsModel.closeToTray)
                Toggle("Minimize to menu bar (Phase 8)", isOn: $settingsModel.minimizeToTray)
                Toggle("Start in menu bar (Phase 8)", isOn: $settingsModel.startInTray)
                Toggle("Notify when window is hidden (Phase 8)", isOn: $settingsModel.trayNotifications)
                Text("Tray behavior is stored now and applied in a later phase.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section("Transfers") {
                Stepper(value: $settingsModel.transferStallTimeoutSec, in: 0 ... 3600, step: 5) {
                    if settingsModel.transferStallTimeoutSec == 0 {
                        Text("Stall timeout: Disabled")
                    } else {
                        Text("Stall timeout: \(settingsModel.transferStallTimeoutSec) sec")
                    }
                }
                Toggle(
                    "Auto-resume interrupted transfer after reconnect",
                    isOn: $settingsModel.autoResumeTransferAfterReconnect
                )
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}
