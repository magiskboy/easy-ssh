// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

@MainActor
final class EasySshAppDelegate: NSObject, NSApplicationDelegate {
    var appModel: AppModel?
    var tray: TrayController?

    func applicationDidFinishLaunching(_ notification: Notification) {
        installApplicationIconIfAvailable()
        NSApp.setActivationPolicy(.regular)
        if tray?.isAvailable == true {
            NSApp.setActivationPolicy(.regular)
        }

        if ESSAppSettings.shared().startInTray, tray?.isAvailable == true {
            hideMainWindows()
        } else {
            tray?.showWindow()
        }

        DispatchQueue.main.async { [weak self] in
            self?.appModel?.workspace.beginRestoreIfNeeded()
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        false
    }

    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if !flag {
            tray?.showWindow()
        }
        return true
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        guard let appModel else { return .terminateNow }
        if !appModel.confirmQuitWithActiveSessions() {
            tray?.forceQuit = false
            return .terminateCancel
        }
        appModel.workspace.saveNow()
        return .terminateNow
    }

    func hideMainWindows() {
        for window in NSApp.windows where window.canBecomeMain {
            window.orderOut(nil)
        }
    }

    private func installApplicationIconIfAvailable() {
        if let icon = Bundle.main.image(forResource: "easy-ssh") {
            NSApp.applicationIconImage = icon
            return
        }
        if let url = Bundle.main.url(forResource: "easy-ssh", withExtension: "icns"),
           let icon = NSImage(contentsOf: url) {
            NSApp.applicationIconImage = icon
            return
        }
        if let url = Bundle.main.url(forResource: "app-256", withExtension: "png"),
           let icon = NSImage(contentsOf: url) {
            NSApp.applicationIconImage = icon
        }
    }
}

struct AppLifecycleBridge: NSViewRepresentable {
    @EnvironmentObject private var appModel: AppModel

    func makeCoordinator() -> Coordinator {
        Coordinator(appModel: appModel)
    }

    func makeNSView(context: Context) -> NSView {
        let view = NSView(frame: .zero)
        DispatchQueue.main.async {
            context.coordinator.attach(to: view.window)
        }
        return view
    }

    func updateNSView(_ nsView: NSView, context: Context) {
        DispatchQueue.main.async {
            context.coordinator.attach(to: nsView.window)
        }
    }

    @MainActor
    final class Coordinator: NSObject, NSWindowDelegate {
        private weak var window: NSWindow?
        private let appModel: AppModel

        init(appModel: AppModel) {
            self.appModel = appModel
        }

        func attach(to window: NSWindow?) {
            guard let window, window !== self.window else { return }
            self.window?.delegate = nil
            self.window = window
            window.delegate = self
        }

        func windowShouldClose(_ sender: NSWindow) -> Bool {
            let settings = ESSAppSettings.shared()
            let tray = appModel.tray

            if tray?.forceQuit == true {
                if !appModel.confirmQuitWithActiveSessions() {
                    tray?.forceQuit = false
                    return false
                }
                appModel.workspace.saveNow()
                return true
            }

            if settings.closeToTray, tray?.isAvailable == true {
                sender.orderOut(nil)
                tray?.maybeShowTrayHint()
                return false
            }

            if !appModel.confirmQuitWithActiveSessions() {
                return false
            }
            appModel.workspace.saveNow()
            return true
        }

        func windowDidMiniaturize(_ notification: Notification) {
            guard ESSAppSettings.shared().minimizeToTray,
                  appModel.tray?.isAvailable == true,
                  let window = notification.object as? NSWindow
            else { return }
            window.deminiaturize(nil)
            window.orderOut(nil)
            appModel.tray?.maybeShowTrayHint()
        }
    }
}
