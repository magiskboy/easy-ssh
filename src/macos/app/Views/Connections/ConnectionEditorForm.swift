// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct ConnectionEditorForm: View {
    @Binding var form: ConnectionFormState
    var isReadOnly: Bool = false

    var body: some View {
        ScrollView {
            Form {
                sessionSection
                connectionSection
                proxySection
                shellSection
            }
            .formStyle(.grouped)
            .disabled(isReadOnly)
        }
    }

    // MARK: - Session

    private var sessionSection: some View {
        Section("Session") {
            TextField("Name", text: $form.name)
            TextField("Host", text: $form.host)
            TextField("Port", value: $form.port, format: .number)
            TextField("Username", text: $form.username)
            TextField("Startup directory", text: $form.startupDirectory)
                .help("Optional remote directory after login")

            Picker("Authentication", selection: $form.usePrivateKey) {
                Text("Password").tag(false)
                Text("Private Key").tag(true)
            }

            if form.usePrivateKey {
                HStack {
                    TextField("Private key path", text: $form.privateKeyPath)
                    if !isReadOnly {
                        Button("Browse…") { pickPrivateKey(forGateway: false) }
                    }
                }
                SecureField("Passphrase", text: $form.passphrase)
                Text("Leave key path empty to use ssh-agent / default identities.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else {
                SecureField("Password", text: $form.password)
                Toggle("Save password in Keychain", isOn: $form.savePassword)
            }

            Toggle("Enable agent forwarding (ForwardAgent)", isOn: $form.agentForwarding)
            Text(
                "Allows remote processes to use keys in your local ssh-agent. "
                    + "Enable only for hosts you trust. This is not a login method and does not "
                    + "replace Gateway/Jump."
            )
            .font(.caption)
            .foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true)

            if form.source == .sshConfig, !form.configAlias.isEmpty {
                LabeledContent("SSH Config Alias", value: form.configAlias)
            }
        }
    }

    // MARK: - Connection

    private var connectionSection: some View {
        Section("Connection") {
            HStack {
                Text("Keep-alive interval")
                Spacer()
                TextField(
                    "seconds",
                    value: $form.keepAliveIntervalSec,
                    format: .number
                )
                .frame(width: 72)
                .multilineTextAlignment(.trailing)
                Text("s")
                    .foregroundStyle(.secondary)
            }
            Text(form.keepAliveIntervalSec == 0 ? "Disabled" : "Send keep-alive every \(form.keepAliveIntervalSec)s")
                .font(.caption)
                .foregroundStyle(.secondary)

            HStack {
                Text("Keep-alive max retries")
                Spacer()
                TextField(
                    "count",
                    value: $form.keepAliveCountMax,
                    format: .number
                )
                .frame(width: 72)
                .multilineTextAlignment(.trailing)
                .disabled(form.keepAliveIntervalSec <= 0)
            }

            Toggle("Enable SSH compression", isOn: $form.compressionEnabled)
        }
    }

    // MARK: - Proxy

    private var proxySection: some View {
        Section("SSH Proxy") {
            Picker("Mode", selection: $form.proxyMode) {
                Text("None").tag(ESSProxyMode.none)
                Text("ProxyJump").tag(ESSProxyMode.proxyJump)
                Text("ProxyCommand").tag(ESSProxyMode.proxyCommand)
            }
            .pickerStyle(.segmented)
            .onChange(of: form.proxyMode) { _, newMode in
                if newMode == .proxyJump, form.jumpHops.isEmpty {
                    form.jumpHops = [JumpHopForm()]
                    form.selectedHopIndex = 0
                }
            }

            if form.proxyMode == .proxyJump {
                Text("Route: local → gateway → target")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                hopList

                if form.jumpHops.indices.contains(form.selectedHopIndex) {
                    hopEditor(index: form.selectedHopIndex)
                }
            }

            if form.proxyMode == .proxyCommand {
                TextField("ProxyCommand", text: $form.proxyCommand)
                    .help("Example: nc -X connect -x 127.0.0.1:9050 %h %p")
                Text("Tokens (expanded by libssh): %h host, %p port, %r user, %n original host")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                Text("Warning: this command runs locally with your user privileges.")
                    .font(.caption)
                    .foregroundStyle(.orange)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    private var hopList: some View {
        VStack(alignment: .leading, spacing: 8) {
            ForEach(Array(form.jumpHops.enumerated()), id: \.element.id) { index, hop in
                Button {
                    form.selectHop(at: index)
                } label: {
                    HStack {
                        Text("Hop \(index + 1): \(hop.displayLabel)")
                            .foregroundStyle(.primary)
                        Spacer()
                        if index == form.selectedHopIndex {
                            Image(systemName: "checkmark.circle.fill")
                                .foregroundStyle(Color.accentColor)
                        }
                    }
                }
                .buttonStyle(.plain)
                .padding(.vertical, 2)
            }

            HStack {
                Button("Add hop") {
                    form.addHop()
                }
                .disabled(isReadOnly)
                Button("Remove hop") {
                    form.removeSelectedHop()
                }
                .disabled(isReadOnly || form.jumpHops.count <= 1)
            }
        }
    }

    @ViewBuilder
    private func hopEditor(index: Int) -> some View {
        let binding = Binding(
            get: { form.jumpHops[index] },
            set: { form.jumpHops[index] = $0 }
        )

        Group {
            TextField("Gateway host", text: binding.host)
            TextField("Gateway port", value: binding.port, format: .number)
            TextField("Gateway username", text: binding.username)
            Toggle("Use same credentials as target", isOn: binding.useTargetCredentials)

            if !binding.wrappedValue.useTargetCredentials {
                Picker("Gateway method", selection: binding.usePrivateKey) {
                    Text("Password").tag(false)
                    Text("Private Key").tag(true)
                }

                if binding.wrappedValue.usePrivateKey {
                    HStack {
                        TextField("Gateway private key", text: binding.privateKeyPath)
                        if !isReadOnly {
                            Button("Browse…") { pickPrivateKey(forGateway: true) }
                        }
                    }
                    // Secrets only apply to hop 0 (core stores one gateway secret).
                    if index == 0 {
                        SecureField("Gateway passphrase", text: $form.gatewayPassphrase)
                    }
                } else if index == 0 {
                    SecureField("Gateway password", text: $form.gatewayPassword)
                }
            }
        }
    }

    // MARK: - SCP / Shell

    private var shellSection: some View {
        Section("SCP / Shell") {
            Toggle(
                "Allow SCP + shell fallback when SFTP is unavailable",
                isOn: $form.shellCommands.allowScpFallback
            )

            TextField("Shell", text: $form.shellCommands.shell)
                .help("Default login shell (e.g. /bin/bash)")

            TextField("Listing command", text: $form.shellCommands.listingCommand)
                .help("e.g. ls -la")
            Toggle("Ignore ls warnings (exit code 1)", isOn: $form.shellCommands.ignoreLsWarnings)
            Toggle("Try ls --full-time", isOn: $form.shellCommands.tryFullTime)

            Toggle("Clear command aliases on connect", isOn: $form.shellCommands.clearAliases)
            Toggle("Clear locale / listing variables", isOn: $form.shellCommands.clearNationalVars)

            TextField("mkdir (%1 = path)", text: $form.shellCommands.mkdirCommand)
            TextField("remove (%1 = path)", text: $form.shellCommands.removeCommand)
            TextField("rename (%1 %2)", text: $form.shellCommands.renameCommand)
            TextField("realpath (%1 = path)", text: $form.shellCommands.realpathCommand)

            if !isReadOnly {
                Button("Reset command set to defaults") {
                    form.shellCommands = .defaults()
                }
            }
        }
    }

    // MARK: - Helpers

    private func pickPrivateKey(forGateway: Bool) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.message = forGateway
            ? "Select a gateway OpenSSH private key"
            : "Select an OpenSSH private key"
        guard panel.runModal() == .OK, let url = panel.url else { return }

        if forGateway {
            guard form.jumpHops.indices.contains(form.selectedHopIndex) else { return }
            form.jumpHops[form.selectedHopIndex].privateKeyPath = url.path
            form.jumpHops[form.selectedHopIndex].usePrivateKey = true
            form.jumpHops[form.selectedHopIndex].useTargetCredentials = false
        } else {
            form.privateKeyPath = url.path
            form.usePrivateKey = true
        }
    }
}
