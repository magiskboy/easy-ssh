// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI

struct ExplorerHubView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        VStack(spacing: 0) {
            if appModel.sessions.isEmpty {
                EmptyStateView(
                    title: "No Session",
                    systemImage: "server.rack",
                    message: "Connect to a host to browse remote explorers.",
                    actionTitle: "New Connection",
                    action: { appModel.openConnectSheet() }
                )
            } else {
                sessionTabs
                Divider()
                if let session = appModel.selectedSession {
                    ExplorerHubPane(session: session)
                        .id(session.id)
                } else {
                    EmptyStateView(
                        title: "Select a Session",
                        systemImage: "rectangle.stack",
                        message: "Choose a session tab to open explorers."
                    )
                }
            }
        }
        .onAppear {
            appModel.selectedSession?.ensureExplorersModel()
        }
        .onChange(of: appModel.selectedSessionId) { _, _ in
            appModel.selectedSession?.ensureExplorersModel()
        }
        .onChange(of: appModel.sidebarMode) { _, mode in
            if mode == .explorers {
                appModel.selectedSession?.ensureExplorersModel()
            } else {
                appModel.selectedSession?.explorers?.stopActive()
            }
        }
        .onDisappear {
            appModel.selectedSession?.explorers?.stopActive()
        }
    }

    private var sessionTabs: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 4) {
                ForEach(appModel.sessions) { session in
                    ExplorerSessionTabChip(
                        title: session.title,
                        isSelected: session.id == appModel.selectedSessionId,
                        state: session.state
                    ) {
                        appModel.selectedSessionId = session.id
                    } onClose: {
                        appModel.closeSession(session.id)
                    }
                }
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 6)
        }
        .background(.bar)
    }
}

private struct ExplorerSessionTabChip: View {
    let title: String
    let isSelected: Bool
    let state: SessionUIState
    let onSelect: () -> Void
    let onClose: () -> Void

    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(stateColor)
                .frame(width: 7, height: 7)
            Text(title)
                .lineLimit(1)
            Button(action: onClose) {
                Image(systemName: "xmark")
                    .font(.caption2.weight(.bold))
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
        .background(isSelected ? Color.accentColor.opacity(0.2) : Color.clear)
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .onTapGesture(perform: onSelect)
    }

    private var stateColor: Color {
        switch state {
        case .connected: return .green
        case .connecting: return .orange
        case .failed: return .red
        case .disconnected, .idle: return .secondary
        }
    }
}

struct ExplorerHubPane: View {
    @ObservedObject var session: SessionViewModel
    @ObservedObject private var explorers: SessionExplorersModel

    init(session: SessionViewModel) {
        self.session = session
        session.ensureExplorersModel()
        self._explorers = ObservedObject(wrappedValue: session.explorers!)
    }

    var body: some View {
        VStack(spacing: 0) {
            Picker("Explorer", selection: $explorers.selectedKind) {
                ForEach(ExplorerKind.allCases) { kind in
                    Text(kind.title).tag(kind)
                }
            }
            .pickerStyle(.segmented)
            .padding(8)
            .onChange(of: explorers.selectedKind) { _, kind in
                explorers.selectKind(kind)
            }

            toolbar
            Divider()

            Group {
                switch explorers.selectedKind {
                case .process:
                    ProcessExplorerTable(explorers: explorers)
                case .container:
                    ContainerExplorerTable(explorers: explorers)
                case .service:
                    ServiceExplorerTable(explorers: explorers)
                case .systemInfo:
                    SystemInfoExplorerView(explorers: explorers)
                }
            }
        }
        .sheet(isPresented: $explorers.showDetailSheet) {
            ExplorerKeyValueSheet(title: explorers.detailTitle, pairs: explorers.detailPairs)
        }
        .sheet(isPresented: $explorers.showInspectSheet) {
            ExplorerKeyValueSheet(
                title: explorers.inspectTitle,
                pairs: explorers.inspectPairs,
                loading: explorers.inspecting
            )
        }
        .onAppear {
            explorers.selectKind(explorers.selectedKind)
        }
    }

    private var toolbar: some View {
        HStack(spacing: 8) {
            if explorers.selectedKind != .systemInfo {
                TextField("Search", text: $explorers.searchText)
                    .textFieldStyle(.roundedBorder)
                    .frame(maxWidth: 240)
            }

            if explorers.selectedKind == .container {
                Picker("Runtime", selection: $explorers.containerRuntimeFilter) {
                    Text("All runtimes").tag("all")
                    ForEach(explorers.containerRuntimes, id: \.self) { runtime in
                        Text(runtime).tag(runtime)
                    }
                }
                .frame(maxWidth: 160)
                Picker("State", selection: $explorers.containerStateFilter) {
                    Text("All states").tag("all")
                    ForEach(explorers.containerStates, id: \.self) { state in
                        Text(state).tag(state)
                    }
                }
                .frame(maxWidth: 140)
            }

            if explorers.selectedKind == .service {
                Picker("Active", selection: $explorers.serviceActiveFilter) {
                    Text("All active").tag("all")
                    ForEach(explorers.serviceActiveStates, id: \.self) { state in
                        Text(state).tag(state)
                    }
                }
                .frame(maxWidth: 140)
                Picker("Enabled", selection: $explorers.serviceEnabledFilter) {
                    Text("All enabled").tag("all")
                    ForEach(explorers.serviceEnabledStates, id: \.self) { state in
                        Text(state).tag(state)
                    }
                }
                .frame(maxWidth: 140)
            }

            Spacer()

            if explorers.busy {
                ProgressView()
                    .controlSize(.small)
            }

            Button {
                explorers.refresh()
            } label: {
                Image(systemName: "arrow.clockwise")
            }
            .help("Refresh")
            .disabled(!explorers.isConnected)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
    }
}

private struct ProcessExplorerTable: View {
    @ObservedObject var explorers: SessionExplorersModel
    @State private var selection: ProcessRow.ID?

    var body: some View {
        ExplorerCapabilityGate(explorers: explorers, hasRows: !explorers.filteredProcesses.isEmpty) {
            Table(explorers.filteredProcesses, selection: $selection) {
                TableColumn("PID") { row in Text("\(row.pid)").monospacedDigit() }
                TableColumn("User") { row in Text(row.user) }
                TableColumn("CPU") { row in Text(String(format: "%.1f", row.cpuPercent)).monospacedDigit() }
                TableColumn("Mem") { row in Text(String(format: "%.1f", row.memPercent)).monospacedDigit() }
                TableColumn("State") { row in Text(row.stateCode) }
                TableColumn("Command") { row in
                    Text(row.command.isEmpty ? row.comm : row.command)
                        .lineLimit(1)
                }
            }
            .contextMenu {
                if let id = selection, let row = explorers.filteredProcesses.first(where: { $0.id == id }) {
                    Button("Details") { explorers.showProcessDetail(row) }
                }
            }
        }
    }
}

private struct ContainerExplorerTable: View {
    @ObservedObject var explorers: SessionExplorersModel
    @State private var selection: ContainerRow.ID?

    var body: some View {
        ExplorerCapabilityGate(explorers: explorers, hasRows: !explorers.filteredContainers.isEmpty) {
            Table(explorers.filteredContainers, selection: $selection) {
                TableColumn("Runtime") { row in Text(row.runtime) }
                TableColumn("Name") { row in Text(row.name.isEmpty ? row.containerId : row.name) }
                TableColumn("Image") { row in Text(row.image).lineLimit(1) }
                TableColumn("State") { row in Text(row.state) }
                TableColumn("CPU") { row in
                    Text(row.cpuPercent < 0 ? "—" : String(format: "%.1f", row.cpuPercent))
                        .monospacedDigit()
                }
                TableColumn("Mem") { row in
                    Text(row.memUsage.isEmpty
                         ? (row.memPercent < 0 ? "—" : String(format: "%.1f", row.memPercent))
                         : row.memUsage)
                        .lineLimit(1)
                }
                TableColumn("Pid") { row in Text(row.pid > 0 ? "\(row.pid)" : "—").monospacedDigit() }
                TableColumn("Id") { row in Text(String(row.containerId.prefix(12))).monospaced() }
            }
            .contextMenu {
                if let id = selection, let row = explorers.filteredContainers.first(where: { $0.id == id }) {
                    Button("Inspect") { explorers.inspectContainer(row) }
                }
            }
            .onChange(of: selection) { _, newValue in
                // Double-click via keyboard Return-like: inspect on selection change is too eager; use context menu.
                _ = newValue
            }
        }
        .onTapGesture(count: 2) {
            if let id = selection, let row = explorers.filteredContainers.first(where: { $0.id == id }) {
                explorers.inspectContainer(row)
            }
        }
    }
}

private struct ServiceExplorerTable: View {
    @ObservedObject var explorers: SessionExplorersModel
    @State private var selection: ServiceRow.ID?

    var body: some View {
        ExplorerCapabilityGate(explorers: explorers, hasRows: !explorers.filteredServices.isEmpty) {
            Table(explorers.filteredServices, selection: $selection) {
                TableColumn("Unit") { row in Text(row.unit) }
                TableColumn("Active") { row in Text(row.activeState) }
                TableColumn("Sub") { row in Text(row.subState) }
                TableColumn("Enabled") { row in Text(row.unitFileState) }
                TableColumn("Description") { row in Text(row.descriptionText).lineLimit(1) }
                TableColumn("Pid") { row in Text(row.mainPid > 0 ? "\(row.mainPid)" : "—").monospacedDigit() }
            }
            .contextMenu {
                if let id = selection, let row = explorers.filteredServices.first(where: { $0.id == id }) {
                    Button("Inspect") { explorers.inspectService(row) }
                    Button("View Logs") { explorers.openServiceLogs(row) }
                }
            }
        }
    }
}

private struct SystemInfoExplorerView: View {
    @ObservedObject var explorers: SessionExplorersModel
    @State private var tab = 0

    var body: some View {
        ExplorerCapabilityGate(explorers: explorers, hasRows: !explorers.systemInfo.isEmpty) {
            VStack(spacing: 0) {
                HStack {
                    Picker("Section", selection: $tab) {
                        Text("Overview").tag(0)
                        Text("CPU & Memory").tag(1)
                        Text("Disk").tag(2)
                        Text("Network").tag(3)
                        Text("GPU").tag(4)
                        Text("Virtualization").tag(5)
                    }
                    .pickerStyle(.segmented)
                    Spacer()
                    Button("Copy Text") { copy(explorers.systemInfoText()) }
                    Button("Copy JSON") { copy(explorers.systemInfoJson()) }
                }
                .padding(8)

                ScrollView {
                    Group {
                        switch tab {
                        case 0: overview
                        case 1: cpuMem
                        case 2: disks
                        case 3: nics
                        case 4: gpus
                        default: virt
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(12)
                }
            }
        }
    }

    private var os: [String: Any] { explorers.systemInfo["os"] as? [String: Any] ?? [:] }
    private var load: [String: Any] { explorers.systemInfo["load"] as? [String: Any] ?? [:] }
    private var cpu: [String: Any] { explorers.systemInfo["cpu"] as? [String: Any] ?? [:] }
    private var mem: [String: Any] { explorers.systemInfo["mem"] as? [String: Any] ?? [:] }

    private var overview: some View {
        VStack(alignment: .leading, spacing: 8) {
            labeled("Host", os["hostname"])
            labeled("OS", os["prettyName"])
            labeled("Kernel", os["kernel"])
            labeled("Arch", os["arch"])
            labeled("Uptime", "\(os["uptimeSec"] ?? "—")s")
            labeled("Load", "\(load["load1"] ?? "—") / \(load["load5"] ?? "—") / \(load["load15"] ?? "—")")
            let usage = (cpu["usagePercent"] as? NSNumber)?.doubleValue ?? -1
            labeled("CPU", usage < 0 ? "—" : String(format: "%.1f%%", usage))
            memBar
        }
    }

    private var cpuMem: some View {
        VStack(alignment: .leading, spacing: 8) {
            labeled("Model", cpu["model"])
            labeled("Logical CPUs", cpu["logicalCpus"])
            labeled("Governor", cpu["governor"])
            memBar
            labeled("Total", formatKiB(mem["totalKb"]))
            labeled("Available", formatKiB(mem["availableKb"]))
            labeled("Free", formatKiB(mem["freeKb"]))
            labeled("Buffers", formatKiB(mem["buffersKb"]))
            labeled("Cached", formatKiB(mem["cachedKb"]))
            labeled("Swap", "\(formatKiB(mem["swapTotalKb"])) total / \(formatKiB(mem["swapFreeKb"])) free")
        }
    }

    private var disks: some View {
        let rows = explorers.systemInfo["disks"] as? [[String: Any]] ?? []
        return VStack(alignment: .leading, spacing: 6) {
            ForEach(Array(rows.enumerated()), id: \.offset) { _, disk in
                Text("\(disk["mountpoint"] ?? "") — \(disk["filesystem"] ?? "") — \(disk["usePercent"] ?? 0)%")
                    .font(.body.monospaced())
            }
            if rows.isEmpty { Text("No disk data").foregroundStyle(.secondary) }
        }
    }

    private var nics: some View {
        let rows = explorers.systemInfo["nics"] as? [[String: Any]] ?? []
        return VStack(alignment: .leading, spacing: 6) {
            ForEach(Array(rows.enumerated()), id: \.offset) { _, nic in
                Text("\(nic["name"] ?? "")  \(nic["operState"] ?? "")  \(nic["ipv4"] ?? "")")
                    .font(.body.monospaced())
            }
            if rows.isEmpty { Text("No NIC data").foregroundStyle(.secondary) }
        }
    }

    private var gpus: some View {
        let rows = explorers.systemInfo["gpus"] as? [[String: Any]] ?? []
        return VStack(alignment: .leading, spacing: 6) {
            ForEach(Array(rows.enumerated()), id: \.offset) { _, gpu in
                Text("\(gpu["name"] ?? "GPU")  util \(gpu["utilGpuPercent"] ?? "—")%  mem \(gpu["memUsedMiB"] ?? "—")/\(gpu["memTotalMiB"] ?? "—") MiB")
                    .font(.body.monospaced())
            }
            if rows.isEmpty { Text("No GPU data").foregroundStyle(.secondary) }
        }
    }

    private var virt: some View {
        let v = explorers.systemInfo["virt"] as? [String: Any] ?? [:]
        return VStack(alignment: .leading, spacing: 8) {
            labeled("Detect", v["detectVirt"])
            labeled("VM", v["vm"])
            labeled("Container", v["container"])
            labeled("Vendor", v["dmiSysVendor"])
            labeled("Product", v["dmiProductName"])
            labeled("Board", "\(v["dmiBoardVendor"] ?? "") \(v["dmiBoardName"] ?? "")")
            labeled("BIOS", "\(v["dmiBiosVendor"] ?? "") \(v["dmiBiosVersion"] ?? "")")
        }
    }

    private var memBar: some View {
        let total = (mem["totalKb"] as? NSNumber)?.doubleValue ?? 0
        let available = (mem["availableKb"] as? NSNumber)?.doubleValue ?? 0
        let usedFrac = total > 0 ? max(0, min(1, (total - available) / total)) : 0
        return VStack(alignment: .leading, spacing: 4) {
            Text(String(format: "Memory %.0f%% used", usedFrac * 100))
            ProgressView(value: usedFrac)
        }
    }

    private func labeled(_ title: String, _ value: Any?) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title)
                .foregroundStyle(.secondary)
                .frame(width: 120, alignment: .leading)
            Text(value.map { "\($0)" } ?? "—")
                .textSelection(.enabled)
        }
    }

    private func formatKiB(_ value: Any?) -> String {
        guard let n = value as? NSNumber else { return "—" }
        let kib = n.doubleValue
        if kib >= 1024 * 1024 {
            return String(format: "%.1f GiB", kib / (1024 * 1024))
        }
        if kib >= 1024 {
            return String(format: "%.1f MiB", kib / 1024)
        }
        return String(format: "%.0f KiB", kib)
    }

    private func copy(_ text: String) {
        guard !text.isEmpty else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
    }
}

private struct ExplorerCapabilityGate<Content: View>: View {
    @ObservedObject var explorers: SessionExplorersModel
    let hasRows: Bool
    @ViewBuilder let content: () -> Content

    var body: some View {
        if !explorers.isConnected {
            EmptyStateView(
                title: "Not Connected",
                systemImage: "bolt.slash",
                message: "Connect the session to load explorer data."
            )
        } else if explorers.capability == .checking && !hasRows {
            EmptyStateView(
                title: "Checking…",
                systemImage: "hourglass",
                message: explorers.capabilityMessage.isEmpty ? "Probing remote host…" : explorers.capabilityMessage
            )
        } else if explorers.capability == .unavailable || explorers.capability == .permissionDenied || explorers.capability == .error {
            EmptyStateView(
                title: explorers.capability.emptyTitle,
                systemImage: "exclamationmark.triangle",
                message: explorers.capabilityMessage.isEmpty ? explorers.lastError : explorers.capabilityMessage
            )
        } else if !hasRows {
            EmptyStateView(
                title: "No Results",
                systemImage: "line.3.horizontal.decrease.circle",
                message: explorers.searchText.isEmpty ? "Waiting for data…" : "No rows match the current filter."
            )
        } else {
            content()
        }
    }
}

private struct ExplorerKeyValueSheet: View {
    let title: String
    let pairs: [(String, String)]
    var loading: Bool = false
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text(title).font(.headline)
                Spacer()
                Button("Close") { dismiss() }
            }
            .padding()
            Divider()
            if loading && pairs.isEmpty {
                ProgressView("Loading…")
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                List(pairs, id: \.0) { pair in
                    HStack(alignment: .top) {
                        Text(pair.0)
                            .foregroundStyle(.secondary)
                            .frame(width: 180, alignment: .leading)
                        Text(pair.1)
                            .textSelection(.enabled)
                    }
                }
            }
        }
        .frame(minWidth: 520, minHeight: 360)
    }
}
