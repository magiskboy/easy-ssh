// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

enum WindowFrameStore {
    private static let key = "easy-ssh-native.windowFrame"

    static func load() -> NSRect? {
        guard let raw = UserDefaults.standard.string(forKey: key) else { return nil }
        let parts = raw.split(separator: ",").compactMap { Double($0.trimmingCharacters(in: .whitespaces)) }
        guard parts.count == 4 else { return nil }
        let rect = NSRect(x: parts[0], y: parts[1], width: parts[2], height: parts[3])
        guard rect.width >= 900, rect.height >= 560 else { return nil }
        return rect
    }

    static func save(_ frame: NSRect) {
        let raw = String(format: "%.1f,%.1f,%.1f,%.1f", frame.origin.x, frame.origin.y, frame.width, frame.height)
        UserDefaults.standard.set(raw, forKey: key)
    }
}

/// Restores and persists the hosting NSWindow frame via UserDefaults.
struct WindowFrameTracker: NSViewRepresentable {
    func makeCoordinator() -> Coordinator {
        Coordinator()
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

    final class Coordinator {
        private var observers: [NSObjectProtocol] = []
        private weak var window: NSWindow?
        private var didRestore = false

        func attach(to window: NSWindow?) {
            guard let window, window !== self.window else { return }
            detach()
            self.window = window

            if !didRestore, let frame = WindowFrameStore.load() {
                window.setFrame(frame, display: true)
                didRestore = true
            }

            let center = NotificationCenter.default
            observers = [
                center.addObserver(
                    forName: NSWindow.didResizeNotification,
                    object: window,
                    queue: .main
                ) { [weak self] note in
                    self?.persist(note.object as? NSWindow)
                },
                center.addObserver(
                    forName: NSWindow.didMoveNotification,
                    object: window,
                    queue: .main
                ) { [weak self] note in
                    self?.persist(note.object as? NSWindow)
                },
            ]
        }

        private func persist(_ window: NSWindow?) {
            guard let window, window.isVisible else { return }
            WindowFrameStore.save(window.frame)
        }

        private func detach() {
            let center = NotificationCenter.default
            for observer in observers {
                center.removeObserver(observer)
            }
            observers.removeAll()
            window = nil
        }

        deinit {
            let center = NotificationCenter.default
            for observer in observers {
                center.removeObserver(observer)
            }
        }
    }
}
