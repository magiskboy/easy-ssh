// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftTerm
import SwiftUI

struct TerminalRepresentable: NSViewRepresentable {
    @ObservedObject var session: SessionViewModel

    func makeCoordinator() -> TerminalCoordinator {
        TerminalCoordinator(session: session)
    }

    func makeNSView(context: Context) -> TerminalView {
        let terminal = TerminalView(frame: .zero)
        terminal.terminalDelegate = context.coordinator
        terminal.configureNativeColors()
        context.coordinator.attach(terminal: terminal)
        return terminal
    }

    func updateNSView(_ nsView: TerminalView, context: Context) {
        context.coordinator.session = session
        if let data = session.pendingTerminalData {
            let bytes = [UInt8](data)
            nsView.feed(byteArray: ArraySlice(bytes))
            DispatchQueue.main.async {
                if session.pendingTerminalData == data {
                    session.pendingTerminalData = nil
                }
            }
        }
    }
}

final class TerminalCoordinator: NSObject, TerminalViewDelegate {
    var session: SessionViewModel
    private weak var terminal: TerminalView?

    init(session: SessionViewModel) {
        self.session = session
    }

    func attach(terminal: TerminalView) {
        self.terminal = terminal
    }

    func sizeChanged(source: TerminalView, newCols: Int, newRows: Int) {
        Task { @MainActor in
            session.resizeTerminal(cols: newCols, rows: newRows)
        }
    }

    func setTerminalTitle(source: TerminalView, title: String) {
        Task { @MainActor in
            if !title.isEmpty {
                session.title = title
            }
        }
    }

    func hostCurrentDirectoryUpdate(source: TerminalView, directory: String?) {}

    func send(source: TerminalView, data: ArraySlice<UInt8>) {
        let payload = Data(data)
        Task { @MainActor in
            session.sendTerminalData(payload)
        }
    }

    func scrolled(source: TerminalView, position: Double) {}

    func clipboardCopy(source: TerminalView, content: Data) {
        if let str = String(data: content, encoding: .utf8) {
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(str, forType: .string)
        }
    }

    func rangeChanged(source: TerminalView, startY: Int, endY: Int) {}
}
