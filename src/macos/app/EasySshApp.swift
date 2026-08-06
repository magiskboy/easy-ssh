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
        WindowGroup("Easy SSH Native") {
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

        Settings {
            SettingsRootView()
                .environmentObject(appModel)
                .environmentObject(settingsModel)
        }
    }
}

private struct AppMenuCommands: Commands {
    @ObservedObject var appModel: AppModel

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("New Connection…") {
                appModel.openConnectSheet()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.newConnection"))

            Button("Browse Connections…") {
                appModel.openConnectionManager()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.connectionManager"))

            Button("Close Session") {
                appModel.closeSelectedSession()
            }
            .keyboardShortcut("w", modifiers: [.command])
            .disabled(appModel.selectedSession == nil)

            Button("Quick Connect…") {
                appModel.openQuickConnect()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.quickConnect"))
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

        CommandMenu("Session") {
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

            Button("Rename Shell…") {
                appModel.renameShellInSelectedSession()
            }
            .disabled(!appModel.canUseTerminalActions)

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

            Divider()

            Button("Container Explorer") {
                appModel.presentExplorer(.container)
            }
            .disabled(appModel.selectedSession?.state != .connected)

            Button("Service Explorer") {
                appModel.presentExplorer(.service)
            }
            .disabled(appModel.selectedSession?.state != .connected)

            Button("Process Explorer") {
                appModel.presentExplorer(.process)
            }
            .disabled(appModel.selectedSession?.state != .connected)

            Button("System Info") {
                appModel.presentExplorer(.systemInfo)
            }
            .disabled(appModel.selectedSession?.state != .connected)

            Divider()

            Button("Disconnect") {
                appModel.disconnectSelectedSession()
            }
            .disabled(appModel.selectedSession?.state != .connected)

            Button("Reconnect") {
                appModel.reconnectSelectedSession()
            }
            .disabled(appModel.selectedSession == nil)

            Divider()

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
        }

        CommandGroup(after: .windowArrangement) {
            Button("Open Log") {
                appModel.openLogFile()
            }

            Button("Settings…") {
                appModel.openSettings(tab: .general)
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.settings"))

            Button("Keyboard Shortcuts…") {
                appModel.openSettings(tab: .shortcuts)
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.shortcuts"))

            Button("Command Palette…") {
                appModel.openCommandPalette()
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.commandPalette"))
        }

        CommandGroup(replacing: .appInfo) {
            Button("About Easy SSH") {
                appModel.activeModal = .about
            }
            .optionalKeyboardShortcut(appModel.shortcutPortable(for: "general.about"))
        }
    }
}
