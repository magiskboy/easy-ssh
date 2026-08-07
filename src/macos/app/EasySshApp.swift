// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

@main
struct EasySshApp: App {
    @NSApplicationDelegateAdaptor(EasySshAppDelegate.self) private var appDelegate
    @StateObject private var appModel = AppModel()
    @StateObject private var settingsModel = SettingsModel()

    init() {
        QtRuntimeBootstrap.start()
    }

    var body: some Scene {
        WindowGroup("Easy SSH", id: "main") {
            ContentView()
                .environmentObject(appModel)
                .environmentObject(settingsModel)
                .frame(minWidth: 900, minHeight: 560)
                .onAppear {
                    appDelegate.appModel = appModel
                    appDelegate.tray = appModel.tray
                }
        }
        .commands {
            AppMenuCommands(appModel: appModel)
        }

        WindowGroup(id: "explorer") {
            ExplorerWindowView()
                .environmentObject(appModel)
                .environmentObject(settingsModel)
        }
        .defaultSize(width: 760, height: 520)
        .windowResizability(.contentMinSize)

        Window("Connection Manager", id: "connectionManager") {
            ConnectionManagerView()
                .environmentObject(appModel)
                .environmentObject(settingsModel)
        }
        .defaultSize(width: 900, height: 640)
        .windowResizability(.contentMinSize)

        Settings {
            SettingsRootView()
                .environmentObject(appModel)
                .environmentObject(settingsModel)
        }
        .defaultSize(width: 480, height: 400)
        .windowResizability(.contentSize)
    }
}

private struct AppMenuCommands: Commands {
    @ObservedObject var appModel: AppModel

    var body: some Commands {
        CommandGroup(replacing: .appInfo) {
            Button("About Easy SSH") {
                appModel.activeModal = .about
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.about"))
        }

        // Do not add CommandGroup(replacing: .appSettings) — the `Settings` scene
        // already injects a Settings… item; replacing still leaves a duplicate on macOS.

        CommandGroup(replacing: .newItem) {
            Button("New Connection…") {
                appModel.openConnectSheet()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.newConnection"))

            Button("Browse Connections…") {
                appModel.openConnectionManager()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.connectionManager"))

            Button("Quick Connect…") {
                appModel.openQuickConnect()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.quickConnect"))

            Divider()

            Button("New Shell") {
                appModel.openShellInSelectedSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "session.newSession"))
            .disabled(!(appModel.selectedSession?.canOpenShell ?? false))

            Button("Close Shell") {
                appModel.closeShellInSelectedSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "shell.close"))
            .disabled(!appModel.canUseTerminalActions)

            Button("Close Session") {
                appModel.closeSelectedSession()
            }
            .keyboardShortcut("w", modifiers: [.command])
            .disabled(appModel.selectedSession == nil)

            Divider()

            Menu("Go to Shell") {
                if let session = appModel.selectedSession, !session.shells.isEmpty {
                    ForEach(session.shells) { shell in
                        Button(shell.title) {
                            appModel.focusShellInSelectedSession(shell.id)
                        }
                    }
                } else {
                    Button("No shells") {}
                        .disabled(true)
                }
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "session.goToShell"))
            .disabled(appModel.selectedSession?.shells.isEmpty ?? true)
        }

        CommandGroup(replacing: .undoRedo) {
            EmptyView()
        }

        CommandGroup(after: .pasteboard) {
            Button("Copy Selection") {
                appModel.copySelectionFromActiveSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "terminal.copy"))
            .disabled(!appModel.canUseTerminalActions)

            Button("Paste to Terminal") {
                appModel.pasteClipboardIntoActiveSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "terminal.paste"))
            .disabled(!appModel.canUseTerminalActions)

            Button("Find in Terminal…") {
                appModel.toggleFindInSelectedSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "terminal.search"))
            .disabled(!appModel.canUseTerminalActions)

            Button("Clear Screen") {
                appModel.clearTerminalInSelectedSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "terminal.clearScreen"))
            .disabled(!appModel.canUseTerminalActions)

            Divider()

            Button("Save Log…") {
                appModel.saveLogInSelectedSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "terminal.saveLog"))
            .disabled(!appModel.canUseTerminalActions)

            Button("Save Screenshot…") {
                appModel.saveScreenshotInSelectedSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "terminal.saveScreenshot"))
            .disabled(!appModel.canUseTerminalActions)
        }

        CommandGroup(replacing: .sidebar) {
            EmptyView()
        }

        CommandGroup(replacing: .toolbar) {
            EmptyView()
        }

        CommandGroup(replacing: .help) {
            EmptyView()
        }

        CommandGroup(after: .windowArrangement) {
            Button("Next Tab") {
                appModel.selectNextSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "session.nextTab"))
            .disabled(appModel.sessions.count < 2)

            Button("Previous Tab") {
                appModel.selectPreviousSession()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "session.previousTab"))
            .disabled(appModel.sessions.count < 2)

            Divider()

            Button("Open Log") {
                appModel.openLogFile()
            }

            Button("Command Palette…") {
                appModel.openCommandPalette()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.commandPalette"))

            Button("Keyboard Shortcuts…") {
                appModel.openSettings(tab: .shortcuts)
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.shortcuts"))
        }
    }
}
