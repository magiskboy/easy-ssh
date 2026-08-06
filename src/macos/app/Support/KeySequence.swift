// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import SwiftUI

/// Parses Qt `QKeySequence::PortableText` (e.g. `Ctrl+Shift+N`, `Meta+,`) for menus and recorders.
enum KeySequence {
    /// On macOS, portable `Ctrl` in stored shortcuts maps to the Command modifier (Qt convention).
    static func keyboardShortcut(fromPortable portable: String?) -> KeyboardShortcut? {
        guard let portable, !portable.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            return nil
        }
        let parts = portable.split(separator: "+").map { String($0).trimmingCharacters(in: .whitespaces) }
        guard let last = parts.last, let key = keyEquivalent(fromToken: last) else { return nil }

        var modifiers: EventModifiers = []
        for part in parts.dropLast() {
            switch part.lowercased() {
            case "ctrl", "control", "meta":
                modifiers.insert(.command)
            case "alt":
                modifiers.insert(.option)
            case "shift":
                modifiers.insert(.shift)
            default:
                break
            }
        }
        return KeyboardShortcut(key, modifiers: modifiers)
    }

    static func portableString(from shortcut: KeyboardShortcut) -> String {
        var parts: [String] = []
        if shortcut.modifiers.contains(.shift) { parts.append("Shift") }
        if shortcut.modifiers.contains(.option) { parts.append("Alt") }
        if shortcut.modifiers.contains(.command) { parts.append("Ctrl") }
        parts.append(token(from: shortcut.key))
        return parts.joined(separator: "+")
    }

    static func displayString(fromPortable portable: String?) -> String {
        guard let shortcut = keyboardShortcut(fromPortable: portable) else {
            return portable ?? ""
        }
        var parts: [String] = []
        if shortcut.modifiers.contains(.control) { parts.append("⌃") }
        if shortcut.modifiers.contains(.option) { parts.append("⌥") }
        if shortcut.modifiers.contains(.shift) { parts.append("⇧") }
        if shortcut.modifiers.contains(.command) { parts.append("⌘") }
        parts.append(displayKey(shortcut.key))
        return parts.joined()
    }

    static func matches(_ press: KeyPress, portable: String?) -> Bool {
        guard let shortcut = keyboardShortcut(fromPortable: portable) else { return false }
        guard pressKeyEquivalent(press.key) == shortcut.key else { return false }
        let mods = press.modifiers
        let wantCommand = shortcut.modifiers.contains(.command)
        let wantShift = shortcut.modifiers.contains(.shift)
        let wantOption = shortcut.modifiers.contains(.option)
        let hasCommand = mods.contains(.command)
        let hasShift = mods.contains(.shift)
        let hasOption = mods.contains(.option)
        return hasCommand == wantCommand && hasShift == wantShift && hasOption == wantOption
    }

    static func capturePortable(from press: KeyPress) -> String? {
        guard let key = pressKeyEquivalent(press.key) else { return nil }
        var mods: EventModifiers = []
        if press.modifiers.contains(.command) { mods.insert(.command) }
        if press.modifiers.contains(.shift) { mods.insert(.shift) }
        if press.modifiers.contains(.option) { mods.insert(.option) }
        let shortcut = KeyboardShortcut(key, modifiers: mods)
        return portableString(from: shortcut)
    }

    // MARK: - Private

    private static func keyEquivalent(fromToken token: String) -> KeyEquivalent? {
        let t = token.trimmingCharacters(in: .whitespaces)
        if t.count == 1, let ch = t.first {
            return KeyEquivalent(ch)
        }
        switch t.lowercased() {
        case "space": return KeyEquivalent(" ")
        case "tab": return KeyEquivalent("\t")
        case "backtab": return KeyEquivalent("\t")
        case "backspace": return KeyEquivalent("\u{8}")
        case "delete", "del": return KeyEquivalent("\u{7F}")
        case "return", "enter": return KeyEquivalent("\r")
        case "escape", "esc": return KeyEquivalent("\u{1B}")
        case "home": return KeyEquivalent("\u{1}")
        case "end": return KeyEquivalent("\u{4}")
        case "pageup": return KeyEquivalent("\u{11}")
        case "pagedown": return KeyEquivalent("\u{12}")
        case "up": return KeyEquivalent("\u{F700}")
        case "down": return KeyEquivalent("\u{F701}")
        case "left": return KeyEquivalent("\u{F702}")
        case "right": return KeyEquivalent("\u{F703}")
        case "f1": return KeyEquivalent("\u{F704}")
        case "f2": return KeyEquivalent("\u{F705}")
        case "f3": return KeyEquivalent("\u{F706}")
        case "f4": return KeyEquivalent("\u{F707}")
        case "f5": return KeyEquivalent("\u{F708}")
        case "f6": return KeyEquivalent("\u{F709}")
        case "f7": return KeyEquivalent("\u{F70A}")
        case "f8": return KeyEquivalent("\u{F70B}")
        case "f9": return KeyEquivalent("\u{F70C}")
        case "f10": return KeyEquivalent("\u{F70D}")
        case "f11": return KeyEquivalent("\u{F70E}")
        case "f12": return KeyEquivalent("\u{F70F}")
        default:
            if t.hasPrefix("F"), let num = Int(t.dropFirst()), (1 ... 35).contains(num) {
                let code = 0xF703 + num
                return KeyEquivalent(Character(UnicodeScalar(code)!))
            }
            return nil
        }
    }

    private static func token(from key: KeyEquivalent) -> String {
        let s = String(key.character)
        switch s {
        case " ": return "Space"
        case "\t": return "Tab"
        case "\u{8}": return "Backspace"
        case "\u{7F}": return "Delete"
        case "\r": return "Return"
        case "\u{1B}": return "Escape"
        case ",": return ","
        default:
            if s.count == 1 { return s.uppercased() }
            return s
        }
    }

    private static func displayKey(_ key: KeyEquivalent) -> String {
        let s = String(key.character)
        if s == "," { return "," }
        if s.count == 1 { return s.uppercased() }
        return s
    }

    private static func pressKeyEquivalent(_ key: KeyEquivalent) -> KeyEquivalent? {
        key
    }
}

struct OptionalKeyboardShortcut: ViewModifier {
    let shortcut: KeyboardShortcut?

    func body(content: Content) -> some View {
        if let shortcut {
            content.keyboardShortcut(shortcut)
        } else {
            content
        }
    }
}

extension View {
    func optionalKeyboardShortcut(_ portable: String?) -> some View {
        modifier(OptionalKeyboardShortcut(shortcut: KeySequence.keyboardShortcut(fromPortable: portable)))
    }
}
