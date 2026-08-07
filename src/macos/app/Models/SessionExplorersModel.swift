// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation
import SwiftUI

enum ExplorerKind: String, CaseIterable, Identifiable {
    case process
    case container
    case service
    case systemInfo

    var id: String { rawValue }

    var title: String {
        switch self {
        case .process: return "Processes"
        case .container: return "Containers"
        case .service: return "Services"
        case .systemInfo: return "System Info"
        }
    }

    var bridgeKind: String { rawValue }
}

enum ExplorerCapabilityState: String {
    case checking
    case available
    case unavailable
    case permissionDenied
    case error

    static func from(_ raw: String) -> ExplorerCapabilityState {
        ExplorerCapabilityState(rawValue: raw) ?? .error
    }

    var emptyTitle: String {
        switch self {
        case .checking: return "Checking…"
        case .available: return "No Results"
        case .unavailable: return "Unavailable"
        case .permissionDenied: return "Permission Denied"
        case .error: return "Error"
        }
    }
}

struct ProcessRow: Identifiable, Hashable {
    let pid: Int64
    let ppid: Int64
    let uid: Int64
    let user: String
    let cpuPercent: Double
    let memPercent: Double
    let stateCode: String
    let nice: Int
    let priority: Int
    let elapsedSeconds: Int64
    let cpuTime: String
    let rssKiB: Int64
    let vszKiB: Int64
    let comm: String
    let command: String

    var id: String { String(pid) }

    static func from(dict: [AnyHashable: Any]) -> ProcessRow? {
        guard let pid = int64(dict["pid"]) else { return nil }
        return ProcessRow(
            pid: pid,
            ppid: int64(dict["ppid"]) ?? 0,
            uid: int64(dict["uid"]) ?? -1,
            user: dict["user"] as? String ?? "",
            cpuPercent: double(dict["cpuPercent"]) ?? 0,
            memPercent: double(dict["memPercent"]) ?? 0,
            stateCode: dict["stateCode"] as? String ?? "",
            nice: intValue(dict["nice"]) ?? 0,
            priority: intValue(dict["priority"]) ?? 0,
            elapsedSeconds: int64(dict["elapsedSeconds"]) ?? -1,
            cpuTime: dict["cpuTime"] as? String ?? "",
            rssKiB: int64(dict["rssKiB"]) ?? 0,
            vszKiB: int64(dict["vszKiB"]) ?? 0,
            comm: dict["comm"] as? String ?? "",
            command: dict["command"] as? String ?? ""
        )
    }

    var detailPairs: [(String, String)] {
        [
            ("PID", "\(pid)"),
            ("PPID", "\(ppid)"),
            ("User", user),
            ("UID", "\(uid)"),
            ("CPU %", String(format: "%.1f", cpuPercent)),
            ("Mem %", String(format: "%.1f", memPercent)),
            ("State", stateCode),
            ("Nice", "\(nice)"),
            ("Priority", "\(priority)"),
            ("Elapsed", elapsedSeconds >= 0 ? "\(elapsedSeconds)s" : "—"),
            ("CPU time", cpuTime.isEmpty ? "—" : cpuTime),
            ("RSS", "\(rssKiB) KiB"),
            ("VSZ", "\(vszKiB) KiB"),
            ("Comm", comm.isEmpty ? "—" : comm),
            ("Command", command.isEmpty ? "—" : command),
        ]
    }
}

struct ContainerRow: Identifiable, Hashable {
    let runtime: String
    let containerId: String
    let name: String
    let image: String
    let state: String
    let pid: Int64
    let runtimeNamespace: String
    let cpuPercent: Double
    let memPercent: Double
    let memUsage: String

    var id: String { "\(runtime):\(containerId)" }

    var asSeedDict: [String: Any] {
        [
            "runtime": runtime,
            "containerId": containerId,
            "name": name,
            "image": image,
            "state": state,
            "pid": pid,
            "runtimeNamespace": runtimeNamespace,
            "cpuPercent": cpuPercent,
            "memPercent": memPercent,
            "memUsage": memUsage,
        ]
    }

    static func from(dict: [AnyHashable: Any]) -> ContainerRow? {
        let runtime = dict["runtime"] as? String ?? ""
        let containerId = dict["containerId"] as? String ?? ""
        guard !runtime.isEmpty || !containerId.isEmpty else { return nil }
        return ContainerRow(
            runtime: runtime,
            containerId: containerId,
            name: dict["name"] as? String ?? "",
            image: dict["image"] as? String ?? "",
            state: dict["state"] as? String ?? "",
            pid: int64(dict["pid"]) ?? 0,
            runtimeNamespace: dict["runtimeNamespace"] as? String ?? "",
            cpuPercent: double(dict["cpuPercent"]) ?? -1,
            memPercent: double(dict["memPercent"]) ?? -1,
            memUsage: dict["memUsage"] as? String ?? ""
        )
    }
}

struct ServiceRow: Identifiable, Hashable {
    let manager: String
    let unit: String
    let descriptionText: String
    let loadState: String
    let activeState: String
    let subState: String
    let unitFileState: String
    let mainPid: Int64

    var id: String { "\(manager):\(unit)" }

    var asSeedDict: [String: Any] {
        [
            "manager": manager,
            "unit": unit,
            "description": descriptionText,
            "loadState": loadState,
            "activeState": activeState,
            "subState": subState,
            "unitFileState": unitFileState,
            "mainPid": mainPid,
        ]
    }

    static func from(dict: [AnyHashable: Any]) -> ServiceRow? {
        let manager = dict["manager"] as? String ?? ""
        let unit = dict["unit"] as? String ?? ""
        guard !unit.isEmpty else { return nil }
        return ServiceRow(
            manager: manager,
            unit: unit,
            descriptionText: dict["description"] as? String ?? "",
            loadState: dict["loadState"] as? String ?? "",
            activeState: dict["activeState"] as? String ?? "",
            subState: dict["subState"] as? String ?? "",
            unitFileState: dict["unitFileState"] as? String ?? "",
            mainPid: int64(dict["mainPid"]) ?? 0
        )
    }
}

@MainActor
final class SessionExplorersModel: ObservableObject {
    @Published var selectedKind: ExplorerKind = .process
    @Published var searchText: String = ""

    @Published private(set) var processRows: [ProcessRow] = []
    @Published private(set) var containerRows: [ContainerRow] = []
    @Published private(set) var serviceRows: [ServiceRow] = []
    @Published private(set) var systemInfo: [String: Any] = [:]

    @Published private(set) var capability: ExplorerCapabilityState = .checking
    @Published private(set) var capabilityMessage: String = ""
    @Published private(set) var busy = false
    @Published private(set) var lastError: String = ""

    @Published var containerRuntimeFilter: String = "all"
    @Published var containerStateFilter: String = "all"
    @Published var serviceActiveFilter: String = "all"
    @Published var serviceEnabledFilter: String = "all"

    @Published var inspectTitle: String = ""
    @Published var inspectPairs: [(String, String)] = []
    @Published var showInspectSheet = false
    @Published var inspecting = false

    @Published var detailTitle: String = ""
    @Published var detailPairs: [(String, String)] = []
    @Published var showDetailSheet = false

    private weak var session: SessionViewModel?
    private var controller: ESSSessionController?
    private var activeKind: ExplorerKind?
    private var wired = false

    var isConnected: Bool { session?.state == .connected }

    var filteredProcesses: [ProcessRow] {
        filterRows(processRows) { row in
            "\(row.pid) \(row.user) \(row.stateCode) \(row.command)"
        }
    }

    var filteredContainers: [ContainerRow] {
        var rows = containerRows
        if containerRuntimeFilter != "all" {
            rows = rows.filter { $0.runtime == containerRuntimeFilter }
        }
        if containerStateFilter != "all" {
            rows = rows.filter { $0.state == containerStateFilter }
        }
        return filterRows(rows) { row in
            "\(row.runtime) \(row.name) \(row.image) \(row.state) \(row.containerId)"
        }
    }

    var filteredServices: [ServiceRow] {
        var rows = serviceRows
        if serviceActiveFilter != "all" {
            rows = rows.filter { $0.activeState == serviceActiveFilter }
        }
        if serviceEnabledFilter != "all" {
            rows = rows.filter { $0.unitFileState == serviceEnabledFilter }
        }
        return filterRows(rows) { row in
            "\(row.unit) \(row.descriptionText) \(row.activeState) \(row.subState)"
        }
    }

    var containerRuntimes: [String] {
        Array(Set(containerRows.map(\.runtime).filter { !$0.isEmpty })).sorted()
    }

    var containerStates: [String] {
        Array(Set(containerRows.map(\.state).filter { !$0.isEmpty })).sorted()
    }

    var serviceActiveStates: [String] {
        Array(Set(serviceRows.map(\.activeState).filter { !$0.isEmpty })).sorted()
    }

    var serviceEnabledStates: [String] {
        Array(Set(serviceRows.map(\.unitFileState).filter { !$0.isEmpty })).sorted()
    }

    func attach(to session: SessionViewModel) {
        if self.session === session, wired {
            if session.state == .connected, activeKind == nil {
                selectKind(selectedKind)
            }
            return
        }
        self.session = session
        self.controller = session.sessionController
        wireCallbacks()
        wired = true
        if session.state == .connected {
            selectKind(selectedKind)
        }
    }

    func onSessionConnected() {
        capability = .checking
        capabilityMessage = ""
        lastError = ""
        selectKind(selectedKind)
    }

    func onSessionDisconnected() {
        stopActive()
        processRows = []
        containerRows = []
        serviceRows = []
        systemInfo = [:]
        capability = .unavailable
        capabilityMessage = "Disconnected"
        busy = false
    }

    func selectKind(_ kind: ExplorerKind) {
        selectedKind = kind
        searchText = ""
        guard isConnected, let controller else {
            capability = .unavailable
            capabilityMessage = "Connect to a session to use explorers."
            return
        }
        if activeKind == kind {
            controller.refreshExplorer(kind.bridgeKind)
            return
        }
        if let previous = activeKind {
            controller.stopExplorer(previous.bridgeKind)
        }
        activeKind = kind
        capability = .checking
        capabilityMessage = "Checking…"
        lastError = ""
        controller.startExplorer(kind.bridgeKind)
    }

    func refresh() {
        guard let controller else { return }
        let kind = activeKind ?? selectedKind
        controller.refreshExplorer(kind.bridgeKind)
    }

    func stopActive() {
        guard let controller else { return }
        if let kind = activeKind {
            controller.stopExplorer(kind.bridgeKind)
        }
        controller.stopAllExplorers()
        activeKind = nil
    }

    func showProcessDetail(_ row: ProcessRow) {
        detailTitle = "Process \(row.pid)"
        detailPairs = row.detailPairs
        showDetailSheet = true
    }

    func inspectContainer(_ row: ContainerRow) {
        guard let controller else { return }
        inspecting = true
        inspectTitle = row.name.isEmpty ? row.containerId : row.name
        inspectPairs = []
        showInspectSheet = true
        controller.inspectContainer(row.asSeedDict)
    }

    func inspectService(_ row: ServiceRow) {
        guard let controller else { return }
        inspecting = true
        inspectTitle = row.unit
        inspectPairs = []
        showInspectSheet = true
        controller.inspectService(row.asSeedDict)
    }

    func openServiceLogs(_ row: ServiceRow) {
        guard let controller, let session else { return }
        let cmd = controller.serviceFollowLogsCommand(row.asSeedDict, lines: 100)
        guard !cmd.isEmpty else {
            lastError = "Logs not supported for this service."
            return
        }
        session.openTerminalForExplorerLogs(title: "Logs: \(row.unit)", command: cmd + "\n")
    }

    func systemInfoText() -> String {
        controller?.systemInfoText() ?? ""
    }

    func systemInfoJson() -> String {
        controller?.systemInfoJson() ?? ""
    }

    // MARK: - Private

    private func filterRows<T>(_ rows: [T], text: (T) -> String) -> [T] {
        let q = searchText.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard !q.isEmpty else { return rows }
        return rows.filter { text($0).lowercased().contains(q) }
    }

    private func wireCallbacks() {
        guard let controller else { return }

        controller.onExplorerCapability = { [weak self] kind, capability, message in
            Task { @MainActor in
                guard let self, kind == self.activeKind?.bridgeKind else { return }
                self.capability = .from(capability)
                self.capabilityMessage = message
            }
        }
        controller.onExplorerBusy = { [weak self] kind, busy in
            Task { @MainActor in
                guard let self, kind == self.activeKind?.bridgeKind else { return }
                self.busy = busy
            }
        }
        controller.onExplorerFailed = { [weak self] kind, message in
            Task { @MainActor in
                guard let self, kind == self.activeKind?.bridgeKind else { return }
                self.lastError = message
            }
        }
        controller.onProcessSnapshot = { [weak self] rows in
            Task { @MainActor in
                guard let self else { return }
                self.processRows = rows.compactMap { ProcessRow.from(dict: $0) }
                if self.activeKind == .process {
                    self.capability = .available
                    self.capabilityMessage = ""
                }
            }
        }
        controller.onContainerSnapshot = { [weak self] rows in
            Task { @MainActor in
                guard let self else { return }
                self.containerRows = rows.compactMap { ContainerRow.from(dict: $0) }
                if self.activeKind == .container {
                    self.capability = .available
                    self.capabilityMessage = ""
                }
            }
        }
        controller.onServiceSnapshot = { [weak self] rows in
            Task { @MainActor in
                guard let self else { return }
                self.serviceRows = rows.compactMap { ServiceRow.from(dict: $0) }
                if self.activeKind == .service {
                    self.capability = .available
                    self.capabilityMessage = ""
                }
            }
        }
        controller.onSystemInfoSnapshot = { [weak self] snapshot in
            Task { @MainActor in
                guard let self else { return }
                var mapped: [String: Any] = [:]
                for (key, value) in snapshot {
                    mapped["\(key)"] = value
                }
                self.systemInfo = mapped
                if self.activeKind == .systemInfo {
                    self.capability = .available
                    self.capabilityMessage = ""
                }
            }
        }
        controller.onContainerInspect = { [weak self] info, error in
            Task { @MainActor in
                guard let self else { return }
                self.inspecting = false
                if let error, !error.isEmpty {
                    self.inspectPairs = [("Error", error)]
                    return
                }
                self.inspectPairs = Self.flatten(info)
            }
        }
        controller.onServiceInspect = { [weak self] info, error in
            Task { @MainActor in
                guard let self else { return }
                self.inspecting = false
                if let error, !error.isEmpty {
                    self.inspectPairs = [("Error", error)]
                    return
                }
                self.inspectPairs = Self.flatten(info)
            }
        }
    }

    private static func flatten(_ dict: [AnyHashable: Any], prefix: String = "") -> [(String, String)] {
        var pairs: [(String, String)] = []
        let keys = dict.keys.map { "\($0)" }.sorted()
        for key in keys {
            let path = prefix.isEmpty ? key : "\(prefix).\(key)"
            guard let value = dict[key] else { continue }
            if let nested = value as? [AnyHashable: Any] {
                pairs.append(contentsOf: flatten(nested, prefix: path))
            } else if let arr = value as? [Any] {
                pairs.append((path, arr.map { "\($0)" }.joined(separator: ", ")))
            } else {
                pairs.append((path, "\(value)"))
            }
        }
        return pairs
    }
}

private func int64(_ value: Any?) -> Int64? {
    if let n = value as? NSNumber { return n.int64Value }
    if let v = value as? Int64 { return v }
    if let v = value as? Int { return Int64(v) }
    if let v = value as? Int32 { return Int64(v) }
    return nil
}

private func intValue(_ value: Any?) -> Int? {
    if let n = value as? NSNumber { return n.intValue }
    if let v = value as? Int { return v }
    if let v = value as? Int64 { return Int(v) }
    return nil
}

private func double(_ value: Any?) -> Double? {
    if let n = value as? NSNumber { return n.doubleValue }
    if let v = value as? Double { return v }
    if let v = value as? Float { return Double(v) }
    return nil
}
