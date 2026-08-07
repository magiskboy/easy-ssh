// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct SettingsRootView: View {
    @EnvironmentObject private var appModel: AppModel
    @EnvironmentObject private var settingsModel: SettingsModel

    var body: some View {
        VStack(spacing: 0) {
            TabView(selection: $settingsModel.selectedTab) {
                GeneralSettingsView()
                    .tabItem { Label("General", systemImage: "gearshape") }
                    .tag(SettingsTab.general)

                FileExplorerSettingsView()
                    .tabItem { Label("File Explorer", systemImage: "folder") }
                    .tag(SettingsTab.fileExplorer)

                TerminalSettingsView()
                    .tabItem { Label("Terminal", systemImage: "terminal") }
                    .tag(SettingsTab.terminal)

                ShortcutsSettingsView()
                    .tabItem { Label("Shortcuts", systemImage: "command") }
                    .tag(SettingsTab.shortcuts)
            }
            .padding(.top, 4)

            Divider()

            HStack {
                Spacer()
                Button("Cancel") {
                    settingsModel.cancelEdits()
                }
                Button("Apply") {
                    settingsModel.applyToStore()
                }
                .keyboardShortcut(.defaultAction)
                Button("OK") {
                    settingsModel.applyToStore()
                    NSApp.keyWindow?.close()
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
        }
        .frame(width: 480, height: 400)
        .onAppear {
            settingsModel.reloadDraftFromStore()
            if let tab = appModel.pendingSettingsTab {
                settingsModel.selectedTab = tab
                appModel.pendingSettingsTab = nil
            }
        }
        .onChange(of: appModel.pendingSettingsTab) { _, tab in
            guard let tab else { return }
            settingsModel.selectedTab = tab
            appModel.pendingSettingsTab = nil
        }
    }
}
