// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

@main
struct EasySshApp: App {
    @StateObject private var appModel = AppModel()

    init() {
        QtRuntimeBootstrap.start()
    }

    var body: some Scene {
        WindowGroup("Easy SSH Native") {
            ContentView()
                .environmentObject(appModel)
                .frame(minWidth: 900, minHeight: 560)
        }
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("New Connection…") {
                    appModel.showConnectSheet = true
                }
                .keyboardShortcut("n", modifiers: [.command])
            }
            CommandGroup(after: .pasteboard) {
                Button("Paste to Terminal") {
                    appModel.pasteClipboardIntoActiveSession()
                }
                .keyboardShortcut("v", modifiers: [.command, .shift])
            }
        }
    }
}
