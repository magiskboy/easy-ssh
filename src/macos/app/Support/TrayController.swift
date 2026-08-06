// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import UserNotifications

enum TrayStatusKind {
    case idle
    case connecting
    case connected
    case warning
}

@MainActor
final class TrayController: NSObject {
    private weak var appModel: AppModel?
    private var statusItem: NSStatusItem?
    private var lastNotificationTime: Date?
    private let notificationCenter = UNUserNotificationCenter.current()

    var forceQuit = false

    init(appModel: AppModel) {
        self.appModel = appModel
        super.init()
        setupStatusItem()
        notificationCenter.requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    var isAvailable: Bool { statusItem != nil }

    func refresh() {
        rebuildMenu()
        updateStatusIcon()
        updateTooltip()
    }

    func showWindow() {
        NSApp.activate(ignoringOtherApps: true)
        for window in NSApp.windows where window.canBecomeMain {
            window.makeKeyAndOrderFront(nil)
        }
    }

    func requestQuit() {
        forceQuit = true
        NSApp.keyWindow?.performClose(nil)
        if NSApp.keyWindow == nil {
            NSApp.terminate(nil)
        }
    }

    func maybeNotify(title: String, message: String) {
        guard ESSAppSettings.shared().trayNotifications else { return }
        guard !isMainWindowVisible else { return }
        if let last = lastNotificationTime, Date().timeIntervalSince(last) < 2 {
            return
        }
        lastNotificationTime = Date()

        let content = UNMutableNotificationContent()
        content.title = title
        content.body = message
        let request = UNNotificationRequest(
            identifier: UUID().uuidString,
            content: content,
            trigger: nil
        )
        notificationCenter.add(request)
    }

    func maybeShowTrayHint() {
        let settings = ESSAppSettings.shared()
        guard !settings.trayMinimizeHintShown else { return }
        settings.trayMinimizeHintShown = true
        appModel?.status.notify(
            title: "Menu Bar",
            message: "Easy SSH keeps running in the menu bar. Click the icon to show the window again.",
            level: .status
        )
    }

    private var isMainWindowVisible: Bool {
        NSApp.windows.contains { $0.isVisible && $0.canBecomeMain }
    }

    private func setupStatusItem() {
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        guard let button = item.button else { return }
        statusItem = item
        button.target = self
        button.action = #selector(statusItemClicked(_:))
        button.sendAction(on: [.leftMouseUp, .rightMouseUp])
        item.menu = buildMenu()
        refresh()
    }

    @objc private func statusItemClicked(_ sender: NSStatusBarButton) {
        let event = NSApp.currentEvent
        if event?.type == .rightMouseUp {
            return
        }
        showWindow()
    }

    private func buildMenu() -> NSMenu {
        let menu = NSMenu()
        menu.delegate = self
        return menu
    }

    private func rebuildMenu() {
        guard let menu = statusItem?.menu else { return }
        menu.removeAllItems()

        let showItem = NSMenuItem(title: "Show Easy SSH", action: #selector(showFromMenu), keyEquivalent: "")
        showItem.target = self
        menu.addItem(showItem)
        menu.addItem(.separator())

        let sessionsMenu = NSMenuItem(title: "Sessions", action: nil, keyEquivalent: "")
        sessionsMenu.submenu = buildSessionsMenu()
        menu.addItem(sessionsMenu)

        let recentMenu = NSMenuItem(title: "Recent", action: nil, keyEquivalent: "")
        recentMenu.submenu = buildRecentMenu()
        menu.addItem(recentMenu)

        menu.addItem(.separator())

        let quitItem = NSMenuItem(title: "Quit", action: #selector(quitFromMenu), keyEquivalent: "q")
        quitItem.keyEquivalentModifierMask = [.command]
        quitItem.target = self
        menu.addItem(quitItem)
    }

    private func buildSessionsMenu() -> NSMenu {
        let menu = NSMenu()
        guard let appModel else { return menu }
        if appModel.sessions.isEmpty {
            let empty = NSMenuItem(title: "No open sessions", action: nil, keyEquivalent: "")
            empty.isEnabled = false
            menu.addItem(empty)
            return menu
        }
        for session in appModel.sessions {
            let label = "\(session.title) — \(session.state.rawValue.capitalized)"
            let item = NSMenuItem(title: label, action: #selector(activateSession(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = session.id
            menu.addItem(item)
        }
        return menu
    }

    private func buildRecentMenu() -> NSMenu {
        let menu = NSMenu()
        guard let appModel else { return menu }
        let recent = appModel.recentConnections
        if recent.isEmpty {
            let empty = NSMenuItem(title: "No recent connections", action: nil, keyEquivalent: "")
            empty.isEnabled = false
            menu.addItem(empty)
            return menu
        }
        for info in recent {
            let title = info.name.isEmpty ? info.displayText : info.name
            let item = NSMenuItem(title: title, action: #selector(openRecent(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = info.connectionId
            menu.addItem(item)
        }
        return menu
    }

    @objc private func showFromMenu() {
        showWindow()
    }

    @objc private func quitFromMenu() {
        requestQuit()
    }

    @objc private func activateSession(_ sender: NSMenuItem) {
        guard let sessionId = sender.representedObject as? UUID else { return }
        appModel?.selectedSessionId = sessionId
        appModel?.sidebarMode = .sessions
        showWindow()
    }

    @objc private func openRecent(_ sender: NSMenuItem) {
        guard let connectionId = sender.representedObject as? UUID else { return }
        showWindow()
        appModel?.connect(withId: connectionId)
    }

    private func updateStatusIcon() {
        guard let button = statusItem?.button else { return }
        let kind = aggregateStatus()
        button.image = icon(for: kind)
    }

    private func updateTooltip() {
        guard let appModel else { return }
        var parts = ["Easy SSH"]
        let connected = appModel.sessions.filter { $0.state == .connected }.count
        let connecting = appModel.sessions.filter { $0.state == .connecting }.count
        let failed = appModel.sessions.filter { $0.state == .failed }.count
        if connected > 0 { parts.append("\(connected) connected") }
        if connecting > 0 { parts.append("\(connecting) connecting") }
        if failed > 0 { parts.append("\(failed) failed") }
        statusItem?.button?.toolTip = parts.joined(separator: " · ")
    }

    private func aggregateStatus() -> TrayStatusKind {
        guard let appModel else { return .idle }
        if appModel.sessions.contains(where: { $0.state == .failed || $0.state == .disconnected }) {
            return .warning
        }
        if appModel.sessions.contains(where: { $0.state == .connecting }) {
            return .connecting
        }
        if appModel.sessions.contains(where: { $0.state == .connected }) {
            return .connected
        }
        return .idle
    }

    private func icon(for kind: TrayStatusKind) -> NSImage {
        let size = NSSize(width: 18, height: 18)
        let image = NSImage(size: size)
        image.lockFocus()
        NSColor.clear.setFill()
        NSRect(origin: .zero, size: size).fill()

        let dotColor: NSColor = switch kind {
        case .idle: .secondaryLabelColor
        case .connecting: NSColor(red: 0.98, green: 0.66, blue: 0.15, alpha: 1)
        case .connected: NSColor(red: 0.26, green: 0.63, blue: 0.28, alpha: 1)
        case .warning: NSColor(red: 0.90, green: 0.22, blue: 0.21, alpha: 1)
        }
        dotColor.setFill()
        let dot = NSBezierPath(ovalIn: NSRect(x: 5, y: 5, width: 8, height: 8))
        dot.fill()
        image.unlockFocus()
        image.isTemplate = kind == .idle
        return image
    }
}

extension TrayController: NSMenuDelegate {
    func menuNeedsUpdate(_ menu: NSMenu) {
        refresh()
    }
}
