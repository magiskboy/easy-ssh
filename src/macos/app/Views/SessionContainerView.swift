// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct SessionContainerView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        Group {
            if appModel.sessions.isEmpty {
                SessionsEmptyView()
            } else if let session = appModel.selectedSession {
                SessionPane(session: session)
                    .id(session.id)
            } else {
                EmptyStateView(
                    title: "Select a Session",
                    systemImage: "rectangle.stack",
                    message: "Choose a connection to focus its terminal workspace."
                )
            }
        }
    }
}

struct SessionPane: View {
    @ObservedObject var session: SessionViewModel

    var body: some View {
        VStack(spacing: 0) {
            if !session.shells.isEmpty {
                shellStrip
                Divider()
            }

            ZStack {
                if let tree = session.layout, !session.shells.isEmpty {
                    ShellSplitView(session: session, node: tree)
                        .opacity(session.state == .connected && session.overlayMessage == nil ? 1 : 0.35)
                } else if session.state == .connected {
                    EmptyStateView(
                        title: "No Shell",
                        systemImage: "terminal",
                        message: "Open a new shell from the Session menu."
                    )
                } else {
                    Color.clear
                }

                if let overlay = session.overlayMessage {
                    VStack(spacing: 12) {
                        Text(overlay)
                            .multilineTextAlignment(.center)
                            .foregroundStyle(.primary)
                            .padding(.horizontal)
                        if session.showReconnect {
                            Button("Reconnect") {
                                session.reconnect()
                            }
                            .keyboardShortcut(.defaultAction)
                        }
                    }
                    .padding(24)
                    .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .sheet(item: $session.renameRequest) { request in
            RenameShellSheet(
                title: Binding(
                    get: { session.renameRequest?.title ?? request.title },
                    set: { newValue in
                        if var r = session.renameRequest {
                            r.title = newValue
                            session.renameRequest = r
                        }
                    }
                ),
                onCancel: { session.renameRequest = nil },
                onSave: {
                    if let r = session.renameRequest {
                        session.commitRename(request: r)
                    }
                }
            )
        }
        .alert(
            "Paste Multiple Lines?",
            isPresented: Binding(
                get: { session.pendingMultilinePaste != nil },
                set: { if !$0 { session.cancelMultilinePaste() } }
            )
        ) {
            Button("Paste", role: .none) {
                session.confirmMultilinePaste()
            }
            Button("Cancel", role: .cancel) {
                session.cancelMultilinePaste()
            }
        } message: {
            let lines = session.pendingMultilinePaste?
                .split(whereSeparator: \.isNewline).count ?? 0
            Text("Clipboard contains \(lines) lines. Paste into the terminal?")
        }
    }

    private var shellStrip: some View {
        ShellTabStripRepresentable(session: session)
            .frame(height: 28)
            .background(.bar)
    }
}

// MARK: - AppKit tab strip
// SwiftUI Button/onTapGesture inside ScrollView is unreliable on macOS (clicks
// silently dropped). Drive select/close with real NSButtons instead.

private struct ShellTabStripRepresentable: NSViewRepresentable {
    @ObservedObject var session: SessionViewModel

    func makeCoordinator() -> Coordinator {
        Coordinator(session: session)
    }

    func makeNSView(context: Context) -> ShellTabStripView {
        let view = ShellTabStripView()
        view.coordinator = context.coordinator
        return view
    }

    func updateNSView(_ nsView: ShellTabStripView, context: Context) {
        context.coordinator.session = session
        nsView.coordinator = context.coordinator
        nsView.reload(
            items: session.shells.map { shell in
                ShellTabItem(
                    id: shell.id,
                    title: shell.title,
                    isSelected: shell.id == session.focusedShellId,
                    isInLayout: session.layout?.contains(shell.id) ?? false
                )
            }
        )
    }

    @MainActor
    final class Coordinator {
        var session: SessionViewModel

        init(session: SessionViewModel) {
            self.session = session
        }

        func select(_ id: UUID) {
            session.focusShell(id)
        }

        func close(_ id: UUID) {
            session.closeShell(id)
        }

        func rename(_ id: UUID) {
            session.beginRenameShell(id)
        }
    }
}

private struct ShellTabItem: Equatable {
    let id: UUID
    let title: String
    let isSelected: Bool
    let isInLayout: Bool
}

private final class ShellTabStripView: NSView {
    weak var coordinator: ShellTabStripRepresentable.Coordinator?
    private let stack = NSStackView()
    private var items: [ShellTabItem] = []
    private var buttons: [UUID: ShellTabButton] = [:]

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        wantsLayer = true

        stack.orientation = .horizontal
        stack.alignment = .centerY
        stack.spacing = 4
        stack.edgeInsets = NSEdgeInsets(top: 3, left: 8, bottom: 3, right: 8)
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)

        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: leadingAnchor),
            stack.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor),
            stack.topAnchor.constraint(equalTo: topAnchor),
            stack.bottomAnchor.constraint(equalTo: bottomAnchor),
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func reload(items: [ShellTabItem]) {
        if self.items == items { return }
        self.items = items

        let ids = Set(items.map(\.id))
        for (id, button) in buttons where !ids.contains(id) {
            stack.removeArrangedSubview(button)
            button.removeFromSuperview()
            buttons[id] = nil
        }

        for (index, item) in items.enumerated() {
            let button: ShellTabButton
            if let existing = buttons[item.id] {
                button = existing
            } else {
                button = ShellTabButton()
                button.target = self
                button.action = #selector(tabClicked(_:))
                button.closeAction = { [weak self] id in
                    self?.coordinator?.close(id)
                }
                button.setContentHuggingPriority(.required, for: .horizontal)
                button.setContentCompressionResistancePriority(.required, for: .horizontal)
                buttons[item.id] = button
                stack.insertArrangedSubview(button, at: min(index, stack.arrangedSubviews.count))
            }
            if stack.arrangedSubviews.firstIndex(of: button) != index {
                stack.removeArrangedSubview(button)
                stack.insertArrangedSubview(button, at: min(index, stack.arrangedSubviews.count))
            }
            button.configure(item: item)
        }
    }

    @objc private func tabClicked(_ sender: ShellTabButton) {
        coordinator?.select(sender.shellId)
    }

    override func menu(for event: NSEvent) -> NSMenu? {
        guard let button = hitTestTab(at: event.locationInWindow) else { return nil }
        let menu = NSMenu()
        let rename = NSMenuItem(title: "Rename…", action: #selector(renameClicked(_:)), keyEquivalent: "")
        rename.target = self
        rename.representedObject = button.shellId
        menu.addItem(rename)
        let close = NSMenuItem(title: "Close", action: #selector(closeClicked(_:)), keyEquivalent: "")
        close.target = self
        close.representedObject = button.shellId
        menu.addItem(close)
        return menu
    }

    private func hitTestTab(at windowPoint: NSPoint) -> ShellTabButton? {
        let point = convert(windowPoint, from: nil)
        return buttons.values.first { button in
            button.frame.contains(button.superview?.convert(point, from: self) ?? point)
                || convert(button.bounds, from: button).contains(point)
        }
    }

    @objc private func renameClicked(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? UUID else { return }
        coordinator?.rename(id)
    }

    @objc private func closeClicked(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? UUID else { return }
        coordinator?.close(id)
    }
}

/// One chip: title button (select) + nested close button. Nested NSButton receives
/// clicks correctly because we forward mouseDown when the hit lands on close.
private final class ShellTabButton: NSView {
    private(set) var shellId = UUID()
    weak var target: AnyObject?
    var action: Selector?
    var closeAction: ((UUID) -> Void)?

    private let titleButton = NSButton()
    private let closeButton = NSButton(frame: .zero)
    private var titleText = ""

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        wantsLayer = true
        layer?.cornerRadius = 5

        titleButton.isBordered = false
        titleButton.setButtonType(.momentaryChange)
        titleButton.bezelStyle = .flexiblePush
        titleButton.focusRingType = .none
        titleButton.font = NSFont.systemFont(ofSize: NSFont.smallSystemFontSize)
        titleButton.alignment = .left
        titleButton.lineBreakMode = .byTruncatingTail
        titleButton.target = self
        titleButton.action = #selector(titleClicked(_:))
        titleButton.translatesAutoresizingMaskIntoConstraints = false
        addSubview(titleButton)

        closeButton.isBordered = false
        closeButton.image = NSImage(systemSymbolName: "xmark", accessibilityDescription: "Close Shell")
        closeButton.symbolConfiguration = NSImage.SymbolConfiguration(pointSize: 9, weight: .bold)
        closeButton.imagePosition = .imageOnly
        closeButton.focusRingType = .none
        closeButton.toolTip = "Close Shell"
        closeButton.target = self
        closeButton.action = #selector(closeClicked(_:))
        closeButton.translatesAutoresizingMaskIntoConstraints = false
        addSubview(closeButton)

        NSLayoutConstraint.activate([
            titleButton.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 8),
            titleButton.topAnchor.constraint(equalTo: topAnchor),
            titleButton.bottomAnchor.constraint(equalTo: bottomAnchor),
            titleButton.trailingAnchor.constraint(equalTo: closeButton.leadingAnchor, constant: -2),

            closeButton.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -4),
            closeButton.centerYAnchor.constraint(equalTo: centerYAnchor),
            closeButton.widthAnchor.constraint(equalToConstant: 16),
            closeButton.heightAnchor.constraint(equalToConstant: 16),

            heightAnchor.constraint(equalToConstant: 22),
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func configure(item: ShellTabItem) {
        shellId = item.id
        titleText = (item.isInLayout ? "" : "⧉ ") + item.title
        titleButton.title = titleText
        titleButton.toolTip = item.isInLayout
            ? item.title
            : "\(item.title) (not in current split layout)"

        let bg = item.isSelected
            ? NSColor.controlAccentColor.withAlphaComponent(0.25)
            : NSColor.secondaryLabelColor.withAlphaComponent(0.12)
        layer?.backgroundColor = bg.cgColor
        invalidateIntrinsicContentSize()
    }

    override var intrinsicContentSize: NSSize {
        let titleWidth = ceil(
            (titleText as NSString).size(withAttributes: [.font: NSFont.systemFont(ofSize: NSFont.smallSystemFontSize)]).width
        )
        return NSSize(width: max(64, titleWidth + 8 + 2 + 16 + 4), height: 22)
    }

    @objc private func titleClicked(_ sender: Any?) {
        guard let target, let action else { return }
        _ = target.perform(action, with: self)
    }

    @objc private func closeClicked(_ sender: Any?) {
        closeAction?(shellId)
    }
}

struct ShellSplitView: View {
    @ObservedObject var session: SessionViewModel
    let node: ShellLayoutNode

    var body: some View {
        switch node {
        case let .leaf(id):
            if let shell = session.shells.first(where: { $0.id == id }) {
                ShellPaneView(session: session, shell: shell)
            } else {
                Color.black.opacity(0.05)
            }
        case let .split(axis, first, second):
            switch axis {
            case .horizontal:
                HSplitView {
                    ShellSplitView(session: session, node: first)
                    ShellSplitView(session: session, node: second)
                }
            case .vertical:
                VSplitView {
                    ShellSplitView(session: session, node: first)
                    ShellSplitView(session: session, node: second)
                }
            }
        }
    }
}

struct ShellPaneView: View {
    @ObservedObject var session: SessionViewModel
    @ObservedObject var shell: ShellViewModel

    var body: some View {
        VStack(spacing: 0) {
            if session.showFindBar && session.focusedShellId == shell.id {
                TerminalFindBar(shell: shell) {
                    session.showFindBar = false
                    shell.showFindBar = false
                    shell.clearFind()
                    session.requestTerminalActivation(for: shell.id)
                }
            }
            // Do not attach SwiftUI tap gestures here — they intercept clicks so the
            // embedded TerminalView never becomes first responder (caret keeps blinking
            // on inactive panes). Focus is synced from HostedTerminalView.mouseDown /
            // becomeFirstResponder instead.
            TerminalRepresentable(session: session, shell: shell)
        }
    }
}

private struct RenameShellSheet: View {
    @Binding var title: String
    let onCancel: () -> Void
    let onSave: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Rename Shell")
                .font(.headline)
            TextField("Title", text: $title)
                .textFieldStyle(.roundedBorder)
            HStack {
                Spacer()
                Button("Cancel", action: onCancel)
                    .keyboardShortcut(.cancelAction)
                Button("Save") {
                    onSave()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
        .padding(20)
        .frame(width: 360)
    }
}
