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
                    appModel.openConnectSheet()
                }
                .keyboardShortcut("n", modifiers: [.command])

                Button("Browse Connections…") {
                    appModel.openConnectionManager()
                }
                .keyboardShortcut("o", modifiers: [.command, .shift])

                Button("Close Session") {
                    appModel.closeSelectedSession()
                }
                .keyboardShortcut("w", modifiers: [.command])
                .disabled(appModel.selectedSession == nil)
            }

            CommandGroup(after: .pasteboard) {
                Button("Copy Selection") {
                    appModel.copySelectionFromActiveSession()
                }
                .keyboardShortcut("c", modifiers: [.command, .shift])
                .disabled(!appModel.canUseTerminalActions)

                Button("Paste to Terminal") {
                    appModel.pasteClipboardIntoActiveSession()
                }
                .keyboardShortcut("v", modifiers: [.command, .shift])
                .disabled(!appModel.canUseTerminalActions)

                Button("Find in Terminal…") {
                    appModel.toggleFindInSelectedSession()
                }
                .keyboardShortcut("f", modifiers: [.command, .shift])
                .disabled(!appModel.canUseTerminalActions)

                Button("Clear Screen") {
                    appModel.clearTerminalInSelectedSession()
                }
                .keyboardShortcut("k", modifiers: [.command, .shift])
                .disabled(!appModel.canUseTerminalActions)

                Divider()

                Button("Save Log…") {
                    appModel.saveLogInSelectedSession()
                }
                .disabled(!appModel.canUseTerminalActions)

                Button("Save Screenshot…") {
                    appModel.saveScreenshotInSelectedSession()
                }
                .disabled(!appModel.canUseTerminalActions)
            }

            CommandMenu("View") {
                ForEach(SidebarMode.allCases) { mode in
                    Button(mode.title) {
                        if mode.isImplemented {
                            appModel.sidebarMode = mode
                        }
                    }
                    .disabled(!mode.isImplemented)
                    .keyboardShortcut(viewShortcut(for: mode), modifiers: [.command])
                }
            }

            CommandMenu("Session") {
                Button("New Shell") {
                    appModel.openShellInSelectedSession()
                }
                .keyboardShortcut("t", modifiers: [.command, .shift])
                .disabled(!(appModel.selectedSession?.canOpenShell ?? false))

                Button("Close Shell") {
                    appModel.closeShellInSelectedSession()
                }
                .keyboardShortcut("w", modifiers: [.command, .shift])
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
                .disabled(appModel.selectedSession?.shells.isEmpty ?? true)

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
                .keyboardShortcut("]", modifiers: [.command, .shift])
                .disabled(appModel.sessions.count < 2)

                Button("Previous Tab") {
                    appModel.selectPreviousSession()
                }
                .keyboardShortcut("[", modifiers: [.command, .shift])
                .disabled(appModel.sessions.count < 2)
            }

            CommandGroup(after: .windowArrangement) {
                Button("Open Log") {
                    appModel.openLogFile()
                }

                Button("Settings…") {}
                    .keyboardShortcut(",", modifiers: [.command])
                    .disabled(true)

                Button("Command Palette…") {}
                    .keyboardShortcut("p", modifiers: [.command, .shift])
                    .disabled(true)
            }

            CommandGroup(replacing: .appInfo) {
                Button("About Easy SSH") {
                    appModel.activeModal = .about
                }
            }
        }
    }

    private func viewShortcut(for mode: SidebarMode) -> KeyEquivalent {
        switch mode {
        case .sessions: return "1"
        case .files: return "2"
        case .tunnels: return "3"
        case .explorers: return "4"
        }
    }
}
