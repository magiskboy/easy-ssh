// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import Foundation
import SwiftTerm
import SwiftUI

@MainActor
final class TerminalViewModel: ObservableObject, Identifiable {
    let id: UUID

    @Published var title: String
    @Published var customTitle: Bool = false
    @Published var cols: Int = 80
    @Published var rows: Int = 24
    @Published var pendingData: Data?
    @Published var feedToken: UInt64 = 0
    @Published var showFindBar: Bool = false
    @Published var findQuery: String = ""
    @Published var findMatchIndex: Int = 0
    @Published var findMatchTotal: Int = 0

    /// Owned SwiftTerm view. Strong so scrollback survives SwiftUI tear-down when a
    /// shell is swapped out of the visible layout (tab switch / overflow panes).
    /// Cleared only when the shell itself is closed via `releaseTerminal()`.
    var terminalView: TerminalView?

    init(id: UUID, title: String) {
        self.id = id
        self.title = title
    }

    func releaseTerminal() {
        terminalView?.terminalDelegate = nil
        if let hosted = terminalView as? HostedTerminalView {
            hosted.onActivated = nil
        }
        terminalView?.removeFromSuperview()
        terminalView = nil
        pendingData = nil
    }

    func enqueueData(_ data: Data) {
        guard !data.isEmpty else { return }
        // Prefer feeding the live view directly so we do not bounce through SwiftUI
        // updateNSView (re-entrant feed ↔ @Published can hang/crash under MOTD bursts).
        // Keep feeding even while the view is off-hierarchy (retained across tab swaps).
        if let terminal = terminalView {
            let bytes = [UInt8](data)
            terminal.feed(byteArray: ArraySlice(bytes))
            return
        }
        if pendingData == nil {
            pendingData = data
        } else {
            pendingData!.append(data)
        }
        feedToken &+= 1
    }

    func setRemoteTitle(_ remote: String) {
        guard !customTitle, !remote.isEmpty else { return }
        title = remote
    }

    func rename(_ name: String) {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        title = trimmed
        customTitle = true
    }

    func copySelection() {
        guard let text = terminalView?.getSelection(), !text.isEmpty else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
    }

    func hasSelection() -> Bool {
        guard let text = terminalView?.getSelection() else { return false }
        return !text.isEmpty
    }

    func clearScreen() {
        guard let terminal = terminalView else { return }
        terminal.getTerminal().resetToInitialState()
        terminal.setNeedsDisplay(terminal.bounds)
    }

    func bufferText() -> String {
        guard let terminal = terminalView else { return "" }
        let t = terminal.getTerminal()
        // Prefer scrollback+viewport via public getText; clamp end row high.
        let endRow = max(t.rows * 50, 10_000)
        let endCol = max(0, t.cols - 1)
        return t.getText(
            start: Position(col: 0, row: 0),
            end: Position(col: endCol, row: endRow)
        )
    }

    func findNext() {
        guard let terminal = terminalView, !findQuery.isEmpty else { return }
        _ = terminal.findNext(findQuery)
        updateFindSummary(on: terminal)
    }

    func findPrevious() {
        guard let terminal = terminalView, !findQuery.isEmpty else { return }
        _ = terminal.findPrevious(findQuery)
        updateFindSummary(on: terminal)
    }

    func clearFind() {
        terminalView?.clearSearch()
        findMatchIndex = 0
        findMatchTotal = 0
    }

    private func updateFindSummary(on terminal: TerminalView) {
        let summary = terminal.searchMatchSummary(findQuery)
        findMatchIndex = summary.index
        findMatchTotal = summary.total
    }
}
