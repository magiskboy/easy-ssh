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
        updateStatusIcon()
        updateTooltip()
    }

    func showWindow() {
        // Status-item menus run inside a tracking session; activating immediately
        // often no-ops. Restore on the next turn after the menu dismisses.
        DispatchQueue.main.async { [weak self] in
            self?.restoreMainWindows()
        }
    }

    private func restoreMainWindows() {
        NSApp.setActivationPolicy(.regular)
        NSApp.unhide(nil)
        NSApp.activate(ignoringOtherApps: true)

        let mains = NSApp.windows.filter { Self.isMainContentWindow($0) }
        if mains.isEmpty {
            appModel?.requestOpenMainWindow()
            NSApp.activate(ignoringOtherApps: true)
            return
        }

        for window in mains {
            if window.isMiniaturized {
                window.deminiaturize(nil)
            }
            window.makeKeyAndOrderFront(nil)
            window.orderFrontRegardless()
        }
        NSApp.activate(ignoringOtherApps: true)
    }

    /// Primary app window(s) — excludes status-item chrome, settings, and helpers.
    static func isMainContentWindow(_ window: NSWindow) -> Bool {
        if window.identifier?.rawValue == AppLifecycleBridge.mainWindowIdentifier {
            return true
        }
        // Fallback before the bridge has tagged the window.
        guard window.level == .normal, window.styleMask.contains(.titled) else { return false }
        let title = window.title
        return title == "Easy SSH" || title.hasPrefix("Easy SSH")
    }

    func requestQuit() {
        forceQuit = true
        // Confirm only in applicationShouldTerminate — do not performClose first
        // or the quit dialog is shown twice.
        NSApp.terminate(nil)
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
        NSApp.windows.contains { $0.isVisible && Self.isMainContentWindow($0) }
    }

    private func setupStatusItem() {
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        guard let button = item.button else { return }
        statusItem = item
        // Left-click opens the menu (includes "Show Easy SSH"). Avoid also wiring
        // button.action — dual menu+action handling races with restore.
        button.target = nil
        button.action = nil
        let menu = NSMenu()
        menu.delegate = self
        item.menu = menu
        refresh()
    }

    private func rebuildMenu(into menu: NSMenu) {
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

        if let base = baseTrayIcon() {
            base.draw(
                in: NSRect(origin: .zero, size: size),
                from: NSRect(origin: .zero, size: base.size),
                operation: .sourceOver,
                fraction: 1
            )
        }

        let dotColor: NSColor? = switch kind {
        case .idle: .secondaryLabelColor
        case .connecting: NSColor(red: 0.98, green: 0.66, blue: 0.15, alpha: 1)
        case .connected: NSColor(red: 0.26, green: 0.63, blue: 0.28, alpha: 1)
        case .warning: NSColor(red: 0.90, green: 0.22, blue: 0.21, alpha: 1)
        }

        if let dotColor, kind != .idle {
            dotColor.setFill()
            let dot = NSBezierPath(ovalIn: NSRect(x: 11, y: 1, width: 6, height: 6))
            dot.fill()

            NSColor.white.setStroke()
            dot.lineWidth = 1.5
            dot.stroke()
        }

        image.unlockFocus()
        image.isTemplate = false
        return image
    }

    private func baseTrayIcon() -> NSImage? {
        if let icon = Bundle.main.image(forResource: "app-256") {
            return icon
        }
        if let url = Bundle.main.url(forResource: "app-256", withExtension: "png"),
           let icon = NSImage(contentsOf: url) {
            return icon
        }
        return NSApp.applicationIconImage
    }
}

extension TrayController: NSMenuDelegate {
    func menuNeedsUpdate(_ menu: NSMenu) {
        rebuildMenu(into: menu)
        updateStatusIcon()
        updateTooltip()
    }
}
