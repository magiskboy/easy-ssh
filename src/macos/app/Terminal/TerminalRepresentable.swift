// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftTerm
import SwiftUI

struct TerminalRepresentable: NSViewRepresentable {
    @ObservedObject var session: SessionViewModel
    @ObservedObject var shell: ShellViewModel

    func makeCoordinator() -> TerminalCoordinator {
        TerminalCoordinator(session: session, shell: shell)
    }

    func makeNSView(context: Context) -> TerminalView {
        let terminal = TerminalView(frame: .zero)
        terminal.terminalDelegate = context.coordinator
        TerminalAppearance.apply(to: terminal)
        context.coordinator.attach(terminal: terminal)
        installContextMenu(on: terminal, coordinator: context.coordinator)
        return terminal
    }

    func updateNSView(_ nsView: TerminalView, context: Context) {
        context.coordinator.session = session
        context.coordinator.shell = shell
        if shell.terminalView !== nsView {
            shell.terminalView = nsView
        }

        if context.coordinator.lastAppearanceEpoch != session.appearanceEpoch {
            TerminalAppearance.apply(to: nsView)
            context.coordinator.lastAppearanceEpoch = session.appearanceEpoch
        }

        // Drain any bytes queued before the NSView existed. Clear first to avoid
        // re-entrant updateNSView while feed() triggers layout.
        if let data = shell.pendingData, !data.isEmpty {
            shell.pendingData = nil
            let bytes = [UInt8](data)
            nsView.feed(byteArray: ArraySlice(bytes))
        }
    }

    @MainActor
    static func dismantleNSView(_ nsView: TerminalView, coordinator: TerminalCoordinator) {
        if coordinator.shell.terminalView === nsView {
            coordinator.shell.terminalView = nil
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
        menu.addItem(withTitle: "New Shell", action: #selector(TerminalCoordinator.contextNewShell(_:)), keyEquivalent: "")
        menu.addItem(withTitle: "Close Shell", action: #selector(TerminalCoordinator.contextCloseShell(_:)), keyEquivalent: "")
        menu.addItem(withTitle: "Rename Shell…", action: #selector(TerminalCoordinator.contextRenameShell(_:)), keyEquivalent: "")
        for item in menu.items {
            item.target = coordinator
        }
        terminal.menu = menu
    }
}

@MainActor
final class TerminalCoordinator: NSObject, @preconcurrency TerminalViewDelegate {
    var session: SessionViewModel
    var shell: ShellViewModel
    private weak var terminal: TerminalView?
    var lastAppearanceEpoch: UInt64 = 0

    init(session: SessionViewModel, shell: ShellViewModel) {
        self.session = session
        self.shell = shell
        super.init()
    }

    func attach(terminal: TerminalView) {
        self.terminal = terminal
        shell.terminalView = terminal
    }

    func sizeChanged(source: TerminalView, newCols: Int, newRows: Int) {
        let cols = max(newCols, 2)
        let rows = max(newRows, 2)
        guard cols != shell.cols || rows != shell.rows else { return }
        session.resizeTerminal(cols: cols, rows: rows, shellId: shell.id)
    }

    func setTerminalTitle(source: TerminalView, title: String) {
        shell.setRemoteTitle(title)
    }

    func hostCurrentDirectoryUpdate(source: TerminalView, directory: String?) {}

    func send(source: TerminalView, data: ArraySlice<UInt8>) {
        session.sendTerminalData(Data(data), shellId: shell.id)
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
        session.focusShell(shell.id)
        shell.copySelection()
    }

    @objc func contextPaste(_ sender: Any?) {
        session.focusShell(shell.id)
        session.pasteClipboard()
    }

    @objc func contextFind(_ sender: Any?) {
        session.focusShell(shell.id)
        session.showFindBar = true
        shell.showFindBar = true
    }

    @objc func contextClear(_ sender: Any?) {
        session.focusShell(shell.id)
        shell.clearScreen()
    }

    @objc func contextSaveLog(_ sender: Any?) {
        session.focusShell(shell.id)
        session.saveLogForFocusedShell()
    }

    @objc func contextSaveScreenshot(_ sender: Any?) {
        session.focusShell(shell.id)
        session.saveScreenshotForFocusedShell()
    }

    @objc func contextNewShell(_ sender: Any?) {
        session.openShell()
    }

    @objc func contextCloseShell(_ sender: Any?) {
        session.closeShell(shell.id)
    }

    @objc func contextRenameShell(_ sender: Any?) {
        session.beginRenameShell(shell.id)
    }
}
