// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct FileExplorerSettingsView: View {
    @EnvironmentObject private var settingsModel: SettingsModel

    var body: some View {
        Form {
            Section("Columns") {
                Toggle("Show Size", isOn: $settingsModel.showSizeColumn)
                Toggle("Show Permissions", isOn: $settingsModel.showPermissionsColumn)
                Toggle("Show Modified", isOn: $settingsModel.showModifiedColumn)
            }

            Section("Display") {
                Toggle("Show hidden files", isOn: $settingsModel.showHiddenFiles)
            }

            Section("Downloads") {
                HStack {
                    TextField("Default download folder (empty = ask every time)", text: $settingsModel.defaultDownloadDir)
                    Button("Browse…") { pickDownloadDir() }
                    Button("Clear") { settingsModel.defaultDownloadDir = "" }
                }
            }
        }
        .formStyle(.grouped)
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
    }

    private func pickDownloadDir() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Choose"
        if !settingsModel.defaultDownloadDir.isEmpty {
            panel.directoryURL = URL(fileURLWithPath: settingsModel.defaultDownloadDir, isDirectory: true)
        }
        if panel.runModal() == .OK, let url = panel.url {
            settingsModel.defaultDownloadDir = url.path
        }
    }
}
