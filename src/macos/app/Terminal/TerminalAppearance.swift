// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftTerm

enum TerminalAppearance {
    static func apply(to terminal: TerminalView, active: Bool = true) {
        let settings = ESSAppSettings.shared()

        let family = settings.terminalFontFamily
        let size = CGFloat(settings.terminalFontPointSize > 0 ? settings.terminalFontPointSize : 13)
        if let font = NSFont(name: family, size: size) {
            terminal.font = font
        } else {
            terminal.font = NSFont.monospacedSystemFont(ofSize: size, weight: .regular)
        }

        applyColorScheme(settings.colorScheme, to: terminal)

        let history = Int(settings.historySize)
        terminal.changeScrollback(history > 0 ? history : nil)

        terminal.scrollerStyle = .legacy

        applyCursor(to: terminal, active: active)
    }

    static func applyCursor(to terminal: TerminalView, active: Bool) {
        let settings = ESSAppSettings.shared()
        let blink = active && settings.cursorBlink
        let style: CursorStyle
        switch Int(settings.cursorShape) {
        case 1:
            style = blink ? .blinkUnderline : .steadyUnderline
        case 2:
            style = blink ? .blinkBar : .steadyBar
        default:
            style = blink ? .blinkBlock : .steadyBlock
        }
        terminal.getTerminal().setCursorStyle(style)
    }

    /// Map QTermWidget-style scheme names to simple SwiftTerm fg/bg.
    /// Full QTermWidget palette parity is a known SwiftTerm gap.
    static func applyColorScheme(_ name: String, to terminal: TerminalView) {
        let key = name.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        switch key {
        case "blackonwhite", "whiteonlightyellow", "papercolorlight", "solarizedlight", "blackonlightyellow":
            terminal.nativeForegroundColor = .black
            terminal.nativeBackgroundColor = .white
        case "native", "system":
            terminal.configureNativeColors()
        case "solarized", "nord", "falcon", "breezemodified", "ubuntu", "tango", "darkpastels", "greenonblack":
            terminal.nativeForegroundColor = .white
            terminal.nativeBackgroundColor = .black
        default:
            // WhiteOnBlack, Linux, DarkPastels, Tango, etc.
            terminal.nativeForegroundColor = .white
            terminal.nativeBackgroundColor = .black
        }
        terminal.setNeedsDisplay(terminal.bounds)
    }
}
