// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct TunnelEditorSheet: View {
    @ObservedObject var tunnels: SessionTunnelsModel
    let session: SessionViewModel
    let context: TunnelEditorContext

    @State private var draft: TunnelDefinition
    @State private var socksPassword: String
    @State private var validationMessage = ""

    init(tunnels: SessionTunnelsModel, session: SessionViewModel, context: TunnelEditorContext) {
        self.tunnels = tunnels
        self.session = session
        self.context = context
        _draft = State(initialValue: context.definition)
        _socksPassword = State(initialValue: context.socksPassword)
    }

    var body: some View {
        VStack(spacing: 0) {
            Form {
                Section("General") {
                    TextField("Name", text: $draft.name)
                    Picker("Type", selection: $draft.type) {
                        ForEach(TunnelTypeModel.allCases) { type in
                            Text(type.title).tag(type)
                        }
                    }
                    Toggle("Enabled", isOn: $draft.enabled)
                }

                Section(localSectionTitle) {
                    if showsLocalKindPicker {
                        Picker("Kind", selection: $draft.localKind) {
                            Text("TCP").tag(TunnelEndpointKindModel.tcp)
                            Text("Unix Socket").tag(TunnelEndpointKindModel.unix)
                        }
                    }
                    if draft.localKind == .unix, showsLocalUnixSocket {
                        socketField(
                            title: "Path",
                            text: $draft.localSocketPath
                        ) {
                            draft.localSocketPath = pickSocketPath(seed: draft.localSocketPath)
                        }
                    } else {
                        TextField(localHostLabel, text: $draft.localHost)
                        TextField(localPortLabel, value: $draft.localPort, format: .number)
                    }
                }

                if showsRemoteSection {
                    Section(remoteSectionTitle) {
                        if showsRemoteKindPicker {
                            Picker("Kind", selection: $draft.remoteKind) {
                                Text("TCP").tag(TunnelEndpointKindModel.tcp)
                                Text("Unix Socket").tag(TunnelEndpointKindModel.unix)
                            }
                        }
                        if draft.remoteKind == .unix, showsRemoteUnixSocket {
                            socketField(
                                title: "Path",
                                text: $draft.remoteSocketPath
                            ) {
                                draft.remoteSocketPath = pickSocketPath(seed: draft.remoteSocketPath)
                            }
                        } else {
                            TextField(remoteHostLabel, text: $draft.remoteHost)
                            TextField(remotePortLabel, value: $draft.remotePort, format: .number)
                        }
                    }
                }

                if draft.type == .dynamic {
                    Section("SOCKS5") {
                        Picker("Authentication", selection: $draft.socksAuth) {
                            ForEach(TunnelSocksAuthModeModel.allCases) { mode in
                                Text(mode.title).tag(mode)
                            }
                        }
                        if draft.socksAuth == .usernamePassword {
                            TextField("Username", text: $draft.socksUsername)
                            SecureField("Password", text: $socksPassword)
                        }
                    }
                }

                if !validationMessage.isEmpty {
                    Section {
                        Text(validationMessage)
                            .foregroundStyle(.red)
                    }
                }
            }

            Divider()

            HStack {
                Spacer()
                Button("Cancel") {
                    tunnels.editor = nil
                }
                Button(context.mode == .create ? "Add Tunnel" : "Save Changes") {
                    save()
                }
                .keyboardShortcut(.defaultAction)
            }
            .padding(12)
        }
        .frame(width: 520, height: 560)
        .onChange(of: draft.type) { _, newType in
            switch newType {
            case .local:
                break
            case .remote:
                draft.remoteKind = .tcp
            case .dynamic:
                draft.localKind = .tcp
                draft.remoteKind = .tcp
            }
        }
    }

    private var localSectionTitle: String {
        switch draft.type {
        case .local: return "Local Bind"
        case .remote: return "Local Destination"
        case .dynamic: return "Local SOCKS Bind"
        }
    }

    private var remoteSectionTitle: String {
        draft.type == .remote ? "Remote Listen" : "Remote Destination"
    }

    private var showsRemoteSection: Bool {
        draft.type != .dynamic
    }

    private var showsLocalKindPicker: Bool {
        draft.type != .dynamic
    }

    private var showsRemoteKindPicker: Bool {
        draft.type == .local
    }

    private var showsLocalUnixSocket: Bool {
        draft.type != .dynamic
    }

    private var showsRemoteUnixSocket: Bool {
        draft.type == .local
    }

    private var localHostLabel: String {
        draft.type == .dynamic ? "Bind Host" : "Host"
    }

    private var localPortLabel: String {
        draft.type == .dynamic ? "Bind Port" : "Port"
    }

    private var remoteHostLabel: String {
        draft.type == .remote ? "Listen Host" : "Host"
    }

    private var remotePortLabel: String {
        draft.type == .remote ? "Listen Port" : "Port"
    }

    @ViewBuilder
    private func socketField(title: String, text: Binding<String>, browse: @escaping () -> Void) -> some View {
        HStack {
            TextField(title, text: text)
            Button("Browse…", action: browse)
        }
    }

    private func save() {
        if let error = tunnels.saveEditor(definition: draft, socksPassword: socksPassword) {
            validationMessage = error
        }
    }

    private func pickSocketPath(seed: String) -> String {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.canCreateDirectories = true
        panel.prompt = "Choose"
        let trimmed = seed.trimmingCharacters(in: .whitespacesAndNewlines)
        if !trimmed.isEmpty {
            panel.directoryURL = URL(fileURLWithPath: (trimmed as NSString).deletingLastPathComponent)
            panel.nameFieldStringValue = (trimmed as NSString).lastPathComponent
        } else {
            panel.directoryURL = URL(fileURLWithPath: "/var/run")
        }
        return panel.runModal() == .OK ? panel.url?.path ?? seed : seed
    }
}
