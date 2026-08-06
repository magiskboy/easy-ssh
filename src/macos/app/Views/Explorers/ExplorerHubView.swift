// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import SwiftUI
import UniformTypeIdentifiers

struct ExplorerHubView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        Group {
            if appModel.sessions.isEmpty {
                EmptyStateView(
                    title: "No Session",
                    systemImage: "server.rack",
                    message: "Connect to a host to browse remote explorers.",
                    actionTitle: "New Connection",
                    action: { appModel.openConnectSheet() }
                )
            } else if let session = appModel.selectedSession {
                ExplorerHubPane(session: session)
                    .id(session.id)
            } else {
                EmptyStateView(
                    title: "Select a Session",
                    systemImage: "rectangle.stack",
                    message: "Choose a connection to open explorers."
                )
            }
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
        ExplorerContentView(explorers: explorers)
            .onAppear {
                explorers.selectKind(explorers.selectedKind)
            }
    }
}

struct ExplorerWindowView: View {
    @EnvironmentObject private var appModel: AppModel

    var body: some View {
        Group {
            if let sessionId = appModel.explorerWindowSessionId,
               let kind = appModel.explorerWindowKind,
               let session = appModel.session(forSessionId: sessionId)
            {
                ExplorerWindowPane(session: session, kind: kind)
                    .id("\(sessionId.uuidString)-\(kind.rawValue)")
            } else {
                EmptyStateView(
                    title: "No Session",
                    systemImage: "server.rack",
                    message: "Connect to a host to browse explorers."
                )
            }
        }
        .frame(minWidth: 760, minHeight: 520)
    }
}

private struct ExplorerWindowPane: View {
    @EnvironmentObject private var appModel: AppModel
    @ObservedObject var session: SessionViewModel
    @ObservedObject private var explorers: SessionExplorersModel
    let kind: ExplorerKind

    init(session: SessionViewModel, kind: ExplorerKind) {
        self.session = session
        self.kind = kind
        session.ensureExplorersModel()
        self._explorers = ObservedObject(wrappedValue: session.explorers!)
    }

    var body: some View {
        NavigationStack {
            ExplorerContentView(explorers: explorers, fixedKind: kind)
                .navigationTitle(kind.title)
                .toolbar {
                    ToolbarItemGroup(placement: .primaryAction) {
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

                        if kind == .systemInfo {
                            SystemInfoShareMenu(
                                explorers: explorers,
                                onStatus: { message, level in
                                    session.onStatus?(message, level)
                                }
                            )
                        }
                    }
                }
        }
        .onAppear {
            explorers.selectKind(kind)
        }
        .onDisappear {
            explorers.stopActive()
            if appModel.explorerWindowSessionId == session.id,
               appModel.explorerWindowKind == kind
            {
                appModel.closeExplorerWindow()
            }
        }
    }
}

private struct ExplorerContentView: View {
    @ObservedObject var explorers: SessionExplorersModel
    let fixedKind: ExplorerKind?

    init(explorers: SessionExplorersModel, fixedKind: ExplorerKind? = nil) {
        self._explorers = ObservedObject(wrappedValue: explorers)
        self.fixedKind = fixedKind
    }

    var body: some View {
        VStack(spacing: 0) {
            if fixedKind == nil {
                Picker("Explorer", selection: $explorers.selectedKind) {
                    ForEach(ExplorerKind.allCases) { kind in
                        Text(kind.title).tag(kind)
                    }
                }
                .pickerStyle(.segmented)
                .labelsHidden()
                .padding(8)
                .onChange(of: explorers.selectedKind) { _, kind in
                    explorers.selectKind(kind)
                }
            }

            if showsInlineToolbar {
                toolbar
                Divider()
            }

            Group {
                switch activeKind {
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
            .frame(maxWidth: .infinity, maxHeight: .infinity)
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
    }

    private var activeKind: ExplorerKind {
        fixedKind ?? explorers.selectedKind
    }

    /// Hub pane keeps filters inline; dedicated windows use the system toolbar for refresh.
    private var showsInlineToolbar: Bool {
        fixedKind == nil || activeKind != .systemInfo
    }

    private var toolbar: some View {
        HStack(spacing: 8) {
            if activeKind != .systemInfo {
                TextField("Search", text: $explorers.searchText)
                    .textFieldStyle(.roundedBorder)
                    .frame(maxWidth: 240)
            }

            if activeKind == .container {
                Picker("Runtime", selection: $explorers.containerRuntimeFilter) {
                    Text("All runtimes").tag("all")
                    ForEach(explorers.containerRuntimes, id: \.self) { runtime in
                        Text(runtime).tag(runtime)
                    }
                }
                .labelsHidden()
                .frame(maxWidth: 160)
                Picker("State", selection: $explorers.containerStateFilter) {
                    Text("All states").tag("all")
                    ForEach(explorers.containerStates, id: \.self) { state in
                        Text(state).tag(state)
                    }
                }
                .labelsHidden()
                .frame(maxWidth: 140)
            }

            if activeKind == .service {
                Picker("Active", selection: $explorers.serviceActiveFilter) {
                    Text("All active").tag("all")
                    ForEach(explorers.serviceActiveStates, id: \.self) { state in
                        Text(state).tag(state)
                    }
                }
                .labelsHidden()
                .frame(maxWidth: 140)
                Picker("Enabled", selection: $explorers.serviceEnabledFilter) {
                    Text("All enabled").tag("all")
                    ForEach(explorers.serviceEnabledStates, id: \.self) { state in
                        Text(state).tag(state)
                    }
                }
                .labelsHidden()
                .frame(maxWidth: 140)
            }

            Spacer()

            if fixedKind == nil {
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
        VStack(spacing: 0) {
            Picker("", selection: $tab) {
                Text("Overview").tag(0)
                Text("CPU & Memory").tag(1)
                Text("Disk").tag(2)
                Text("Network").tag(3)
                Text("GPU").tag(4)
                Text("Virtualization").tag(5)
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            .padding(8)

            ExplorerCapabilityGate(explorers: explorers, hasRows: !explorers.systemInfo.isEmpty) {
                Group {
                    switch tab {
                    case 0:
                        ScrollView {
                            overview
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(12)
                        }
                    case 1:
                        ScrollView {
                            cpuMem
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(12)
                        }
                    case 2:
                        disks
                    case 3:
                        nics
                    case 4:
                        gpus
                    default:
                        virt
                    }
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
            labeled("Host", stringValue(os["hostname"]))
            labeled("OS", stringValue(os["prettyName"]))
            labeled("Kernel", stringValue(os["kernel"]))
            labeled("Arch", stringValue(os["arch"]))
            labeled("Uptime", "\(stringValue(os["uptimeSec"]))s")
            labeled(
                "Load",
                "\(stringValue(load["load1"])) / \(stringValue(load["load5"])) / \(stringValue(load["load15"]))"
            )
        }
    }

    private var cpuMem: some View {
        let totalKb = doubleValue(mem["totalKb"])
        let availableKb = doubleValue(mem["availableKb"])
        let usedKb = totalKb > 0 ? max(0, totalKb - availableKb) : 0
        let memFrac = totalKb > 0 ? min(1, usedKb / totalKb) : 0
        let swapTotalKb = doubleValue(mem["swapTotalKb"])
        let swapFreeKb = doubleValue(mem["swapFreeKb"])
        let swapUsedKb = swapTotalKb > 0 ? max(0, swapTotalKb - swapFreeKb) : 0
        let swapFrac = swapTotalKb > 0 ? min(1, swapUsedKb / swapTotalKb) : 0
        let usage = doubleValue(cpu["usagePercent"])

        return VStack(alignment: .leading, spacing: 16) {
            VStack(alignment: .leading, spacing: 8) {
                Text("CPU").font(.headline)
                labeled("Model", stringValue(cpu["model"]))
                labeled("Logical CPUs", stringValue(cpu["logicalCpus"]))
                labeled("Governor", stringValue(cpu["governor"]))
                if usage >= 0 {
                    labeled("Usage", String(format: "%.1f%%", usage))
                    ProgressView(value: min(1, usage / 100))
                }
            }

            VStack(alignment: .leading, spacing: 8) {
                Text("Memory").font(.headline)
                labeled(
                    "Breakdown",
                    "buffers \(formatKiB(mem["buffersKb"])) · cached \(formatKiB(mem["cachedKb"])) · shmem \(formatKiB(mem["shmemKb"]))"
                )
                labeled(
                    "Used",
                    "\(formatKiBValue(usedKb)) / \(formatKiB(mem["totalKb"])) (\(Int((memFrac * 100).rounded()))%)"
                )
                ProgressView(value: memFrac)
                labeled("Available", formatKiB(mem["availableKb"]))
                labeled("Free", formatKiB(mem["freeKb"]))
            }

            VStack(alignment: .leading, spacing: 8) {
                Text("Swap").font(.headline)
                labeled(
                    "Used",
                    swapTotalKb > 0
                        ? "\(formatKiBValue(swapUsedKb)) / \(formatKiB(mem["swapTotalKb"])) (\(Int((swapFrac * 100).rounded()))%)"
                        : "—"
                )
                ProgressView(value: swapFrac)
            }
        }
    }

    private var disks: some View {
        let rows = diskRows
        return Group {
            if rows.isEmpty {
                emptyTablePlaceholder("No disk data")
            } else {
                Table(rows) {
                    TableColumn("Filesystem") { row in Text(row.filesystem) }
                    TableColumn("Mount") { row in Text(row.mountpoint) }
                    TableColumn("Size") { row in Text(row.size).monospacedDigit() }
                    TableColumn("Used") { row in Text(row.used).monospacedDigit() }
                    TableColumn("Avail") { row in Text(row.avail).monospacedDigit() }
                    TableColumn("Use%") { row in Text(row.usePercent).monospacedDigit() }
                }
            }
        }
    }

    private var nics: some View {
        let rows = nicRows
        return Group {
            if rows.isEmpty {
                emptyTablePlaceholder("No NIC data")
            } else {
                Table(rows) {
                    TableColumn("Interface") { row in Text(row.name) }
                    TableColumn("State") { row in Text(row.state) }
                    TableColumn("IPv4") { row in Text(row.ipv4) }
                    TableColumn("IPv6") { row in Text(row.ipv6).lineLimit(1) }
                    TableColumn("Speed") { row in Text(row.speed).monospacedDigit() }
                    TableColumn("MTU") { row in Text(row.mtu).monospacedDigit() }
                    TableColumn("RX") { row in Text(row.rx).monospacedDigit() }
                    TableColumn("TX") { row in Text(row.tx).monospacedDigit() }
                }
            }
        }
    }

    private var gpus: some View {
        let rows = gpuRows
        return Group {
            if rows.isEmpty {
                emptyTablePlaceholder("No GPU data")
            } else {
                Table(rows) {
                    TableColumn("Index") { row in Text(row.index).monospacedDigit() }
                    TableColumn("Name") { row in Text(row.name) }
                    TableColumn("Util") { row in Text(row.util).monospacedDigit() }
                    TableColumn("Memory") { row in Text(row.memory).monospacedDigit() }
                    TableColumn("Temp") { row in Text(row.temp).monospacedDigit() }
                    TableColumn("Power") { row in Text(row.power).monospacedDigit() }
                    TableColumn("P-state") { row in Text(row.pstate) }
                    TableColumn("Clocks") { row in Text(row.clocks).monospacedDigit() }
                    TableColumn("Driver") { row in Text(row.driver) }
                }
            }
        }
    }

    private var virt: some View {
        Table(virtRows) {
            TableColumn("Property") { row in
                Text(row.property)
                    .foregroundStyle(.secondary)
            }
            TableColumn("Value") { row in
                Text(row.value)
                    .textSelection(.enabled)
            }
        }
    }

    private var diskRows: [SystemInfoDiskRow] {
        let raw = explorers.systemInfo["disks"] as? [[String: Any]] ?? []
        return raw.enumerated().map { index, disk in
            SystemInfoDiskRow(
                id: index,
                filesystem: stringValue(disk["filesystem"]),
                mountpoint: stringValue(disk["mountpoint"]),
                size: formatKiB(disk["sizeKb"]),
                used: formatKiB(disk["usedKb"]),
                avail: formatKiB(disk["availKb"]),
                usePercent: formatPercent(disk["usePercent"])
            )
        }
    }

    private var nicRows: [SystemInfoNicRow] {
        let raw = explorers.systemInfo["nics"] as? [[String: Any]] ?? []
        return raw.enumerated().map { index, nic in
            let rxBps = doubleValue(nic["rxBps"])
            let txBps = doubleValue(nic["txBps"])
            return SystemInfoNicRow(
                id: index,
                name: stringValue(nic["name"]),
                state: stringValue(nic["operState"]),
                ipv4: stringValue(nic["ipv4"]),
                ipv6: stringValue(nic["ipv6"]),
                speed: formatLinkSpeed(nic["speedMbps"]),
                mtu: stringValue(nic["mtu"]),
                rx: rxBps >= 0 ? formatRateBps(rxBps) : formatBytes(nic["rxBytes"]),
                tx: txBps >= 0 ? formatRateBps(txBps) : formatBytes(nic["txBytes"])
            )
        }
    }

    private var gpuRows: [SystemInfoGpuRow] {
        let raw = explorers.systemInfo["gpus"] as? [[String: Any]] ?? []
        return raw.enumerated().map { index, gpu in
            let used = intValue(gpu["memUsedMiB"])
            let total = intValue(gpu["memTotalMiB"])
            let powerDraw = doubleValue(gpu["powerDrawW"])
            let powerLimit = doubleValue(gpu["powerLimitW"])
            let sm = intValue(gpu["clockSmMHz"])
            let memClock = intValue(gpu["clockMemMHz"])
            return SystemInfoGpuRow(
                id: index,
                index: stringValue(gpu["index"]),
                name: stringValue(gpu["name"], fallback: "GPU"),
                util: formatPercent(gpu["utilGpuPercent"]),
                memory: used >= 0 && total >= 0 ? "\(used)/\(total) MiB" : "—",
                temp: formatCelsius(gpu["tempCelsius"]),
                power: powerDraw >= 0 && powerLimit >= 0
                    ? String(format: "%.0f/%.0f W", powerDraw, powerLimit)
                    : "—",
                pstate: stringValue(gpu["pstate"]),
                clocks: sm >= 0 && memClock >= 0 ? "\(sm)/\(memClock) MHz" : "—",
                driver: stringValue(gpu["driverVersion"])
            )
        }
    }

    private var virtRows: [SystemInfoVirtRow] {
        let v = explorers.systemInfo["virt"] as? [String: Any] ?? [:]
        let board = [stringValue(v["dmiBoardVendor"]), stringValue(v["dmiBoardName"])]
            .filter { $0 != "—" }
            .joined(separator: " ")
        let chassis = [stringValue(v["dmiChassisVendor"]), stringValue(v["dmiChassisType"])]
            .filter { $0 != "—" }
            .joined(separator: " ")
        let bios = [
            stringValue(v["dmiBiosVendor"]),
            stringValue(v["dmiBiosVersion"]),
            stringValue(v["dmiBiosDate"]),
        ]
        .filter { $0 != "—" }
        .joined(separator: " ")
        let product = [stringValue(v["dmiProductName"]), stringValue(v["dmiProductVersion"])]
            .filter { $0 != "—" }
            .joined(separator: " ")

        return [
            SystemInfoVirtRow(id: 0, property: "Detected", value: stringValue(v["detectVirt"])),
            SystemInfoVirtRow(id: 1, property: "VM type", value: stringValue(v["vm"])),
            SystemInfoVirtRow(id: 2, property: "Container type", value: stringValue(v["container"])),
            SystemInfoVirtRow(id: 3, property: "Running as VM", value: yesNo(v["isVm"])),
            SystemInfoVirtRow(id: 4, property: "Running as container", value: yesNo(v["isContainer"])),
            SystemInfoVirtRow(id: 5, property: "CPU hypervisor flag", value: yesNo(v["cpuHypervisorFlag"])),
            SystemInfoVirtRow(id: 6, property: "CPU vendor", value: stringValue(v["cpuVendor"])),
            SystemInfoVirtRow(id: 7, property: "Docker (.dockerenv)", value: yesNo(v["dockerEnv"])),
            SystemInfoVirtRow(id: 8, property: "Podman (.containerenv)", value: yesNo(v["podmanEnv"])),
            SystemInfoVirtRow(id: 9, property: "WSL", value: yesNo(v["wsl"])),
            SystemInfoVirtRow(id: 10, property: "Init cgroup", value: stringValue(v["cgroupInit"])),
            SystemInfoVirtRow(id: 11, property: "System vendor", value: stringValue(v["dmiSysVendor"])),
            SystemInfoVirtRow(id: 12, property: "Product", value: product.isEmpty ? "—" : product),
            SystemInfoVirtRow(id: 13, property: "Board", value: board.isEmpty ? "—" : board),
            SystemInfoVirtRow(id: 14, property: "Chassis", value: chassis.isEmpty ? "—" : chassis),
            SystemInfoVirtRow(id: 15, property: "BIOS", value: bios.isEmpty ? "—" : bios),
        ]
    }

    private func emptyTablePlaceholder(_ message: String) -> some View {
        Text(message)
            .foregroundStyle(.secondary)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
    }

    private func labeled(_ title: String, _ value: String) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title)
                .foregroundStyle(.secondary)
                .frame(width: 120, alignment: .leading)
            Text(value)
                .textSelection(.enabled)
        }
    }

    private func stringValue(_ value: Any?, fallback: String = "—") -> String {
        guard let value else { return fallback }
        if let text = value as? String {
            let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
            return trimmed.isEmpty ? fallback : trimmed
        }
        if let number = value as? NSNumber {
            return number.stringValue
        }
        return fallback
    }

    private func doubleValue(_ value: Any?) -> Double {
        (value as? NSNumber)?.doubleValue ?? -1
    }

    private func intValue(_ value: Any?) -> Int {
        (value as? NSNumber)?.intValue ?? -1
    }

    private func yesNo(_ value: Any?) -> String {
        guard let number = value as? NSNumber else { return "—" }
        return number.boolValue ? "Yes" : "No"
    }

    private func formatKiB(_ value: Any?) -> String {
        guard let n = value as? NSNumber else { return "—" }
        return formatKiBValue(n.doubleValue)
    }

    private func formatKiBValue(_ kib: Double) -> String {
        if kib < 0 { return "—" }
        if kib >= 1024 * 1024 {
            return String(format: "%.1f GiB", kib / (1024 * 1024))
        }
        if kib >= 1024 {
            return String(format: "%.1f MiB", kib / 1024)
        }
        return String(format: "%.0f KiB", kib)
    }

    private func formatBytes(_ value: Any?) -> String {
        guard let n = value as? NSNumber else { return "—" }
        let bytes = n.doubleValue
        if bytes < 0 { return "—" }
        if bytes >= 1024 * 1024 * 1024 {
            return String(format: "%.1f GiB", bytes / (1024 * 1024 * 1024))
        }
        if bytes >= 1024 * 1024 {
            return String(format: "%.1f MiB", bytes / (1024 * 1024))
        }
        if bytes >= 1024 {
            return String(format: "%.1f KiB", bytes / 1024)
        }
        return String(format: "%.0f B", bytes)
    }

    private func formatRateBps(_ bps: Double) -> String {
        if bps < 0 { return "—" }
        if bps >= 1024 * 1024 {
            return String(format: "%.1f MiB/s", bps / (1024 * 1024))
        }
        if bps >= 1024 {
            return String(format: "%.1f KiB/s", bps / 1024)
        }
        return String(format: "%.0f B/s", bps)
    }

    private func formatPercent(_ value: Any?) -> String {
        let pct = doubleValue(value)
        return pct < 0 ? "—" : String(format: "%.1f%%", pct)
    }

    private func formatCelsius(_ value: Any?) -> String {
        let c = doubleValue(value)
        return c < 0 ? "—" : String(format: "%.1f °C", c)
    }

    private func formatLinkSpeed(_ value: Any?) -> String {
        let mbps = doubleValue(value)
        if mbps < 0 { return "—" }
        if mbps >= 1000 {
            return String(format: "%.1f Gbps", mbps / 1000)
        }
        return String(format: "%.0f Mbps", mbps)
    }
}

private struct SystemInfoDiskRow: Identifiable {
    let id: Int
    let filesystem: String
    let mountpoint: String
    let size: String
    let used: String
    let avail: String
    let usePercent: String
}

private struct SystemInfoNicRow: Identifiable {
    let id: Int
    let name: String
    let state: String
    let ipv4: String
    let ipv6: String
    let speed: String
    let mtu: String
    let rx: String
    let tx: String
}

private struct SystemInfoGpuRow: Identifiable {
    let id: Int
    let index: String
    let name: String
    let util: String
    let memory: String
    let temp: String
    let power: String
    let pstate: String
    let clocks: String
    let driver: String
}

private struct SystemInfoVirtRow: Identifiable {
    let id: Int
    let property: String
    let value: String
}

private struct SystemInfoShareMenu: View {
    @ObservedObject var explorers: SessionExplorersModel
    let onStatus: (String, StatusLevel) -> Void

    private var textSnapshot: String { explorers.systemInfoText() }
    private var jsonSnapshot: String { explorers.systemInfoJson() }
    private var hasSnapshot: Bool { !textSnapshot.isEmpty || !jsonSnapshot.isEmpty }

    private var fileBaseName: String {
        let os = explorers.systemInfo["os"] as? [String: Any] ?? [:]
        let host = (os["hostname"] as? String)?
            .trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let raw = host.isEmpty ? "system-info" : "system-info-\(host)"
        let cleaned = raw.replacingOccurrences(
            of: "[^A-Za-z0-9._-]+",
            with: "-",
            options: .regularExpression
        )
        return cleaned.isEmpty ? "system-info" : cleaned
    }

    var body: some View {
        Menu {
            if let textURL = temporaryShareURL(contents: textSnapshot, fileName: "\(fileBaseName).txt") {
                ShareLink(
                    item: textURL,
                    subject: Text("System Info"),
                    message: Text("System information snapshot"),
                    preview: SharePreview(
                        textURL.lastPathComponent,
                        image: Image(systemName: "doc.text")
                    )
                ) {
                    Label("Share Text…", systemImage: "doc.text")
                }
            }

            if let jsonURL = temporaryShareURL(contents: jsonSnapshot, fileName: "\(fileBaseName).json") {
                ShareLink(
                    item: jsonURL,
                    subject: Text("System Info JSON"),
                    message: Text("System information snapshot (JSON)"),
                    preview: SharePreview(
                        jsonURL.lastPathComponent,
                        image: Image(systemName: "curlybraces")
                    )
                ) {
                    Label("Share JSON…", systemImage: "curlybraces")
                }
            }

            Divider()

            Button("Copy Text") {
                copyToPasteboard(textSnapshot, label: "text")
            }
            .disabled(textSnapshot.isEmpty)

            Button("Copy JSON") {
                copyToPasteboard(jsonSnapshot, label: "JSON")
            }
            .disabled(jsonSnapshot.isEmpty)

            Divider()

            Button("Save Text…") {
                saveSnapshot(textSnapshot, fileName: "\(fileBaseName).txt", contentType: .plainText)
            }
            .disabled(textSnapshot.isEmpty)

            Button("Save JSON…") {
                saveSnapshot(jsonSnapshot, fileName: "\(fileBaseName).json", contentType: .json)
            }
            .disabled(jsonSnapshot.isEmpty)
        } label: {
            Image(systemName: "square.and.arrow.up")
        }
        .help("Share System Info")
        .disabled(!hasSnapshot)
    }

    private func temporaryShareURL(contents: String, fileName: String) -> URL? {
        guard !contents.isEmpty else { return nil }
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(fileName)
        do {
            try contents.write(to: url, atomically: true, encoding: .utf8)
            return url
        } catch {
            return nil
        }
    }

    private func copyToPasteboard(_ text: String, label: String) {
        guard !text.isEmpty else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
        onStatus("Copied System Info \(label)", .success)
    }

    private func saveSnapshot(_ text: String, fileName: String, contentType: UTType) {
        guard !text.isEmpty else { return }
        let panel = NSSavePanel()
        panel.allowedContentTypes = [contentType]
        panel.nameFieldStringValue = fileName
        panel.canCreateDirectories = true
        panel.begin { response in
            guard response == .OK, let url = panel.url else { return }
            do {
                try text.write(to: url, atomically: true, encoding: .utf8)
                DispatchQueue.main.async {
                    onStatus("Saved System Info: \(url.lastPathComponent)", .success)
                }
            } catch {
                DispatchQueue.main.async {
                    onStatus("Could not save System Info: \(error.localizedDescription)", .error)
                }
            }
        }
    }
}

private struct ExplorerCapabilityGate<Content: View>: View {
    @ObservedObject var explorers: SessionExplorersModel
    let hasRows: Bool
    @ViewBuilder let content: () -> Content

    private var isErrorState: Bool {
        explorers.capability == .unavailable
            || explorers.capability == .permissionDenied
            || explorers.capability == .error
    }

    private var isLoading: Bool {
        guard explorers.isConnected, !isErrorState else { return false }
        if explorers.capability == .checking && !hasRows { return true }
        if explorers.busy && !hasRows { return true }
        return false
    }

    var body: some View {
        ZStack {
            if !explorers.isConnected {
                EmptyStateView(
                    title: "Not Connected",
                    systemImage: "bolt.slash",
                    message: "Connect the session to load explorer data."
                )
            } else if isErrorState {
                EmptyStateView(
                    title: explorers.capability.emptyTitle,
                    systemImage: "exclamationmark.triangle",
                    message: explorers.capabilityMessage.isEmpty ? explorers.lastError : explorers.capabilityMessage
                )
            } else {
                content()
                    .opacity(hasRows || isLoading ? 1 : 0)
                    .allowsHitTesting(hasRows)

                if isLoading {
                    ProgressView()
                        .controlSize(.regular)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                        .background(.background.opacity(0.001))
                } else if !hasRows {
                    EmptyStateView(
                        title: "No Results",
                        systemImage: "line.3.horizontal.decrease.circle",
                        message: explorers.searchText.isEmpty
                            ? "No data available."
                            : "No rows match the current filter."
                    )
                }
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

private struct ExplorerKeyValueSheet: View {
    let title: String
    let pairs: [(String, String)]
    var loading: Bool = false

    var body: some View {
        NavigationStack {
            Group {
                if loading && pairs.isEmpty {
                    ProgressView()
                        .controlSize(.regular)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if pairs.isEmpty {
                    EmptyStateView(
                        title: "No Details",
                        systemImage: "doc.text",
                        message: "Nothing to show."
                    )
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
            .navigationTitle(title)
        }
        .frame(minWidth: 520, minHeight: 360)
    }
}
