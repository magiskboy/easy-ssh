// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftTerm
import SwiftUI

/// SwiftTerm view that syncs session focus when the user clicks a pane.
/// AppKit first-responder transfer (via `makeFirstResponder`) makes only the
/// active caret blink; inactive terminals keep a steady visible caret outline.
final class HostedTerminalView: TerminalView {
    var onActivated: (() -> Void)?

    override func mouseDown(with event: NSEvent) {
        // SwiftUI hosting can skip AppKit's automatic first-responder transfer;
        // take key focus explicitly so inactive panes stop blinking.
        window?.makeFirstResponder(self)
        onActivated?()
        super.mouseDown(with: event)
    }
}

struct TerminalRepresentable: NSViewRepresentable {
    @ObservedObject var session: SessionViewModel
    @ObservedObject var terminal: TerminalViewModel

    func makeCoordinator() -> TerminalCoordinator {
        TerminalCoordinator(session: session, terminal: terminal)
    }

    func makeNSView(context: Context) -> HostedTerminalView {
        // Reuse the model-owned TerminalView so scrollback survives layout swaps
        // (SwiftUI tears down NSViewRepresentable when a leaf leaves the tree).
        let hosted: HostedTerminalView
        if let existing = terminal.terminalView as? HostedTerminalView {
            existing.removeFromSuperview()
            hosted = existing
        } else {
            hosted = HostedTerminalView(frame: .zero)
            hosted.wantsLayer = true
            hosted.layer?.masksToBounds = true
            terminal.terminalView = hosted
        }
        hosted.terminalDelegate = context.coordinator
        hosted.onActivated = { [weak coordinator = context.coordinator] in
            guard let coordinator else { return }
            coordinator.session.focusTerminal(coordinator.terminal.id, activateTerminal: false)
        }
        let isActive = session.focusedTerminalId == terminal.id
        TerminalAppearance.apply(to: hosted, active: isActive)
        context.coordinator.lastCursorActive = isActive
        context.coordinator.attach(hosted: hosted)
        installContextMenu(on: hosted, coordinator: context.coordinator)
        return hosted
    }

    func updateNSView(_ nsView: HostedTerminalView, context: Context) {
        context.coordinator.session = session
        context.coordinator.terminal = terminal
        nsView.onActivated = { [weak coordinator = context.coordinator] in
            guard let coordinator else { return }
            coordinator.session.focusTerminal(coordinator.terminal.id, activateTerminal: false)
        }
        if terminal.terminalView !== nsView {
            terminal.terminalView = nsView
        }

        let isActive = session.focusedTerminalId == terminal.id
        if context.coordinator.lastAppearanceEpoch != session.appearanceEpoch {
            TerminalAppearance.apply(to: nsView, active: isActive)
            context.coordinator.lastAppearanceEpoch = session.appearanceEpoch
        } else if context.coordinator.lastCursorActive != isActive {
            TerminalAppearance.applyCursor(to: nsView, active: isActive)
        }
        context.coordinator.lastCursorActive = isActive

        // Drain any bytes queued before the NSView existed. Clear first to avoid
        // re-entrant updateNSView while feed() triggers layout.
        if let data = terminal.pendingData, !data.isEmpty {
            terminal.pendingData = nil
            let bytes = [UInt8](data)
            nsView.feed(byteArray: ArraySlice(bytes))
        }

        // Complete a deferred activation after layout swap / first attach.
        if session.pendingTerminalActivationId == terminal.id {
            _ = session.flushPendingTerminalActivation()
        }
    }

    @MainActor
    static func dismantleNSView(_ nsView: HostedTerminalView, coordinator: TerminalCoordinator) {
        // Keep the TerminalView (and its buffer) owned by TerminalViewModel across
        // temporary removals from the SwiftUI hierarchy. Only clear the activation
        // callback so a detached view cannot retarget focus.
        nsView.onActivated = nil
        if nsView.terminalDelegate === coordinator {
            nsView.terminalDelegate = nil
        }
    }

    private func installContextMenu(on terminal: TerminalView, coordinator: TerminalCoordinator) {
        let menu = NSMenu(title: "Terminal")
        menu.addItem(withTitle: "Copy", action: #selector(TerminalCoordinator.contextCopy(_:)), keyEquivalent: "")
        menu.addItem(withTitle: "Paste", action: #selector(TerminalCoordinator.contextPaste(_:)), keyEquivalent: "")
        menu.addItem(NSMenuItem.separator())
        menu.addItem(withTitle: "Find…", action: #selector(TerminalCoordinator.contextFind(_:)), keyEquivalent: "")
        menu.addItem(withTitle: "Clear", action: #selector(TerminalCoordinator.contextClear(_:)), keyEquivalent: "")
        menu.addItem(NSMenuItem.separator())
        menu.addItem(withTitle: "Save Log…", action: #selector(TerminalCoordinator.contextSaveLog(_:)), keyEquivalent: "")
        menu.addItem(
            withTitle: "Save Screenshot…",
            action: #selector(TerminalCoordinator.contextSaveScreenshot(_:)),
            keyEquivalent: ""
        )
        menu.addItem(NSMenuItem.separator())
        menu.addItem(withTitle: "New Terminal", action: #selector(TerminalCoordinator.contextNewTerminal(_:)), keyEquivalent: "")
        menu.addItem(withTitle: "Close Terminal", action: #selector(TerminalCoordinator.contextCloseTerminal(_:)), keyEquivalent: "")
        menu.addItem(withTitle: "Rename Terminal…", action: #selector(TerminalCoordinator.contextRenameTerminal(_:)), keyEquivalent: "")
        for item in menu.items {
            item.target = coordinator
        }
        terminal.menu = menu
    }
}

@MainActor
final class TerminalCoordinator: NSObject, @preconcurrency TerminalViewDelegate {
    var session: SessionViewModel
    var terminal: TerminalViewModel
    private weak var hostedView: TerminalView?
    var lastAppearanceEpoch: UInt64 = 0
    var lastCursorActive: Bool?

    init(session: SessionViewModel, terminal: TerminalViewModel) {
        self.session = session
        self.terminal = terminal
        super.init()
    }

    func attach(hosted: TerminalView) {
        hostedView = hosted
        terminal.terminalView = hosted
    }

    func sizeChanged(source: TerminalView, newCols: Int, newRows: Int) {
        let cols = max(newCols, 2)
        let rows = max(newRows, 2)
        guard cols != terminal.cols || rows != terminal.rows else { return }
        session.resizeTerminal(cols: cols, rows: rows, terminalId: terminal.id)
    }

    func setTerminalTitle(source: TerminalView, title: String) {
        terminal.setRemoteTitle(title)
    }

    func hostCurrentDirectoryUpdate(source: TerminalView, directory: String?) {}

    func send(source: TerminalView, data: ArraySlice<UInt8>) {
        session.sendTerminalData(Data(data), terminalId: terminal.id)
    }

    func scrolled(source: TerminalView, position: Double) {}

    func clipboardCopy(source: TerminalView, content: Data) {
        if let str = String(data: content, encoding: .utf8) {
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(str, forType: .string)
        }
    }

    func rangeChanged(source: TerminalView, startY: Int, endY: Int) {}

    // MARK: - Context menu

    @objc func contextCopy(_ sender: Any?) {
        session.focusTerminal(terminal.id)
        terminal.copySelection()
    }

    @objc func contextPaste(_ sender: Any?) {
        session.focusTerminal(terminal.id)
        session.pasteClipboard()
    }

    @objc func contextFind(_ sender: Any?) {
        session.focusTerminal(terminal.id, activateTerminal: false)
        session.showFindBar = true
        terminal.showFindBar = true
    }

    @objc func contextClear(_ sender: Any?) {
        session.focusTerminal(terminal.id)
        terminal.clearScreen()
    }

    @objc func contextSaveLog(_ sender: Any?) {
        session.focusTerminal(terminal.id)
        session.saveLogForFocusedTerminal()
    }

    @objc func contextSaveScreenshot(_ sender: Any?) {
        session.focusTerminal(terminal.id)
        session.saveScreenshotForFocusedTerminal()
    }

    @objc func contextNewTerminal(_ sender: Any?) {
        session.openTerminal()
    }

    @objc func contextCloseTerminal(_ sender: Any?) {
        session.closeTerminal(terminal.id)
    }

    @objc func contextRenameTerminal(_ sender: Any?) {
        session.beginRenameTerminal(terminal.id)
    }
}
