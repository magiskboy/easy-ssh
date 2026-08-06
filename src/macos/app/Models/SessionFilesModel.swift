// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import AppKit
import Foundation

struct RemoteFileEntry: Identifiable, Hashable {
    let id: String
    let name: String
    let path: String
    let isDir: Bool
    let isSymlink: Bool
    let linkIsDir: Bool
    let linkTarget: String
    let size: Int64
    let permissions: String
    let mtime: TimeInterval
    let isParentNav: Bool

    var isNavigableDirectory: Bool {
        if isParentNav { return true }
        if isSymlink { return linkIsDir }
        return isDir
    }

    static func parent(of cwd: String) -> RemoteFileEntry {
        let parentPath = Self.parentPath(of: cwd)
        return RemoteFileEntry(
            id: parentPath,
            name: "..",
            path: parentPath,
            isDir: true,
            isSymlink: false,
            linkIsDir: false,
            linkTarget: "",
            size: 0,
            permissions: "",
            mtime: 0,
            isParentNav: true
        )
    }

    static func from(dict: [AnyHashable: Any]) -> RemoteFileEntry? {
        guard let name = dict["name"] as? String,
              let path = dict["path"] as? String
        else { return nil }
        let isDir = (dict["isDir"] as? NSNumber)?.boolValue ?? false
        let isSymlink = (dict["isSymlink"] as? NSNumber)?.boolValue ?? false
        let linkIsDir = (dict["linkIsDir"] as? NSNumber)?.boolValue ?? false
        let linkTarget = dict["linkTarget"] as? String ?? ""
        let size = (dict["size"] as? NSNumber)?.int64Value ?? 0
        let permissions = dict["permissions"] as? String ?? ""
        let mtime = (dict["mtime"] as? NSNumber)?.doubleValue ?? 0
        return RemoteFileEntry(
            id: path,
            name: name,
            path: path,
            isDir: isDir,
            isSymlink: isSymlink,
            linkIsDir: linkIsDir,
            linkTarget: linkTarget,
            size: size,
            permissions: permissions,
            mtime: mtime,
            isParentNav: false
        )
    }

    static func parentPath(of path: String) -> String {
        if path == "/" || path.isEmpty { return "/" }
        var trimmed = path
        while trimmed.count > 1, trimmed.hasSuffix("/") {
            trimmed.removeLast()
        }
        guard let idx = trimmed.lastIndex(of: "/") else { return "/" }
        if idx == trimmed.startIndex { return "/" }
        return String(trimmed[..<idx])
    }

    static func join(dir: String, name: String) -> String {
        if dir == "/" { return "/\(name)" }
        if dir.hasSuffix("/") { return dir + name }
        return "\(dir)/\(name)"
    }
}

struct TransferJobInfo: Equatable {
    let direction: Int
    let localPath: String
    let remoteFinalPath: String
    let bytesDone: Int64
    let bytesTotal: Int64
    let backend: Int
    let lastMessage: String

    static func from(dict: [AnyHashable: Any]) -> TransferJobInfo {
        TransferJobInfo(
            direction: (dict["direction"] as? NSNumber)?.intValue ?? 0,
            localPath: dict["localPath"] as? String ?? "",
            remoteFinalPath: dict["remoteFinalPath"] as? String ?? "",
            bytesDone: (dict["bytesDone"] as? NSNumber)?.int64Value ?? 0,
            bytesTotal: (dict["bytesTotal"] as? NSNumber)?.int64Value ?? 0,
            backend: (dict["backend"] as? NSNumber)?.intValue ?? 0,
            lastMessage: dict["lastMessage"] as? String ?? ""
        )
    }

    var summary: String {
        let name = (remoteFinalPath as NSString).lastPathComponent
        if bytesTotal > 0 {
            return "\(name) — \(bytesDone) / \(bytesTotal) bytes"
        }
        if !lastMessage.isEmpty { return lastMessage }
        return name.isEmpty ? "Interrupted transfer" : name
    }
}

enum FsBackendKind: Int {
    case none = 0
    case sftp = 1
    case scp = 2

    var label: String {
        switch self {
        case .none: return ""
        case .sftp: return "SFTP"
        case .scp: return "SCP + shell"
        }
    }
}

@MainActor
final class SessionFilesModel: ObservableObject {
    @Published private(set) var cwd: String = ""
    @Published private(set) var entries: [RemoteFileEntry] = []
    @Published private(set) var isListing: Bool = false
    @Published private(set) var unavailableReason: String?
    @Published private(set) var fsBackend: FsBackendKind = .none
    @Published private(set) var isTransferring: Bool = false
    @Published private(set) var transferBytesDone: Int64 = 0
    @Published private(set) var transferBytesTotal: Int64 = 0
    @Published private(set) var transferCurrentName: String = ""
    @Published private(set) var transferInterrupted: Bool = false
    @Published private(set) var transferResumable: Bool = false
    @Published private(set) var interruptedJob: TransferJobInfo?
    @Published private(set) var opInFlight: Bool = false
    @Published private(set) var statusMessage: String = ""
    @Published var selectedPaths: Set<String> = []
    @Published var pathBarText: String = ""

    @Published var mkdirPrompt: String?
    @Published var renamePrompt: RenamePrompt?
    @Published var symlinkPrompt: SymlinkPrompt?
    @Published var deleteConfirmNames: [String]?
    @Published var overwriteConfirm: OverwriteConfirm?

    struct RenamePrompt: Identifiable {
        let id = UUID()
        let path: String
        var name: String
    }

    struct SymlinkPrompt: Identifiable {
        let id = UUID()
        let targetName: String
        var linkName: String
    }

    struct OverwriteConfirm: Identifiable {
        let id = UUID()
        let conflicts: [String]
        let localPaths: [String]
        let remoteDir: String
    }

    private weak var session: SessionViewModel?
    private var controller: ESSSessionController?
    private var allEntries: [RemoteFileEntry] = []
    private var pendingRootRequest: String?
    private var pendingNavPath: String?
    private var navPreviousPath: String?
    private var pendingResolvePath: String?
    private var refreshAfterOp: String?
    private var awaitingSftpResult = false
    private var openWithQueue: [(remotePath: String, localDir: String, fileName: String)] = []
    private var openWithActive = false
    private var pendingDeleteQueue: [String] = []

    func attach(to session: SessionViewModel) {
        guard self.session == nil else {
            if session.state == .connected, cwd.isEmpty, unavailableReason == nil {
                startBrowsing()
            }
            return
        }
        self.session = session
        self.controller = session.sessionController
        wireCallbacks()
        if session.state == .connected {
            startBrowsing()
        }
    }

    func onSessionConnected() {
        unavailableReason = nil
        startBrowsing()
    }

    func onSessionDisconnected() {
        if transferResumable || transferInterrupted {
            statusMessage = "Connection lost — reconnect to resume"
            return
        }
        clearListing()
        statusMessage = "Disconnected"
    }

    // MARK: - Navigation

    func startBrowsing() {
        guard let session, session.state == .connected, let controller else { return }
        unavailableReason = nil
        let startup = session.connection.startupDirectory.trimmingCharacters(in: .whitespacesAndNewlines)
        let request = startup.isEmpty ? "." : startup
        pendingRootRequest = request
        isListing = true
        pathBarText = "Resolving…"
        controller.canonicalizePath(request)
    }

    func refresh() {
        guard !cwd.isEmpty, let controller else { return }
        isListing = true
        statusMessage = "Refreshing…"
        controller.listDirectory(cwd)
    }

    func goToPathBar() {
        let trimmed = pathBarText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        navigateTo(trimmed)
    }

    func activate(_ entry: RemoteFileEntry) {
        if entry.isSymlink, !entry.isParentNav {
            guard !opInFlight, let controller else { return }
            pendingResolvePath = entry.path
            opInFlight = true
            controller.resolveEntry(entry.path)
            return
        }
        if entry.isNavigableDirectory {
            navigateTo(entry.path)
            return
        }
        selectedPaths = [entry.path]
        openWithSelected()
    }

    func navigateTo(_ path: String) {
        guard !path.isEmpty, let controller else { return }
        if path != cwd {
            navPreviousPath = cwd
            pendingNavPath = path
        }
        cwd = path
        pathBarText = path
        selectedPaths = []
        isListing = true
        opInFlight = false
        controller.listDirectory(path)
        statusMessage = "Browsing \(path)"
    }

    // MARK: - Actions

    func uploadFiles() {
        guard canMutate else { return }
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = true
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.begin { [weak self] response in
            guard response == .OK else { return }
            Task { @MainActor in
                self?.beginUpload(localPaths: panel.urls.map(\.path))
            }
        }
    }

    func uploadFolder() {
        guard canMutate else { return }
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.urls.first else { return }
            Task { @MainActor in
                self?.beginUpload(localPaths: [url.path])
            }
        }
    }

    func downloadSelected() {
        guard canMutate, !selectedDownloadablePaths.isEmpty else { return }
        let settings = ESSAppSettings.shared()
        let preferred = settings.defaultDownloadDir.trimmingCharacters(in: .whitespacesAndNewlines)
        if !preferred.isEmpty, FileManager.default.fileExists(atPath: preferred) {
            startDownload(remotePaths: selectedDownloadablePaths, localDir: preferred)
            return
        }
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Download"
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.urls.first else { return }
            Task { @MainActor in
                self?.startDownload(remotePaths: self?.selectedDownloadablePaths ?? [], localDir: url.path)
            }
        }
    }

    func openWithSelected() {
        guard canMutate else { return }
        let files = selectedEntries.filter { !$0.isParentNav && !$0.isNavigableDirectory }
        guard !files.isEmpty, let session else { return }
        let root = URL(fileURLWithPath: NSTemporaryDirectory())
            .appendingPathComponent("easy-ssh", isDirectory: true)
            .appendingPathComponent(session.id.uuidString, isDirectory: true)
        openWithQueue.removeAll()
        for entry in files {
            let hash = String(abs(entry.path.hashValue), radix: 16)
            let localDir = root.appendingPathComponent(hash, isDirectory: true).path
            try? FileManager.default.createDirectory(atPath: localDir, withIntermediateDirectories: true)
            openWithQueue.append((entry.path, localDir, entry.name))
        }
        openWithActive = true
        startNextOpenWith()
    }

    func promptMkdir() {
        guard canMutate else { return }
        mkdirPrompt = ""
    }

    func confirmMkdir() {
        guard let name = mkdirPrompt?.trimmingCharacters(in: .whitespacesAndNewlines),
              !name.isEmpty,
              !name.contains("/"),
              let controller
        else {
            mkdirPrompt = nil
            return
        }
        mkdirPrompt = nil
        let path = RemoteFileEntry.join(dir: cwd, name: name)
        opInFlight = true
        awaitingSftpResult = true
        refreshAfterOp = cwd
        controller.createDirectory(path)
        statusMessage = "Creating folder…"
    }

    func promptRename() {
        guard canMutate, let entry = selectedEntries.first(where: { !$0.isParentNav }) else { return }
        renamePrompt = RenamePrompt(path: entry.path, name: entry.name)
    }

    func confirmRename() {
        guard let prompt = renamePrompt,
              let controller
        else { return }
        let newName = prompt.name.trimmingCharacters(in: .whitespacesAndNewlines)
        renamePrompt = nil
        guard !newName.isEmpty, !newName.contains("/"), newName != (prompt.path as NSString).lastPathComponent else {
            return
        }
        let dest = RemoteFileEntry.join(dir: cwd, name: newName)
        opInFlight = true
        awaitingSftpResult = true
        refreshAfterOp = cwd
        controller.renamePath(prompt.path, to: dest)
        statusMessage = "Renaming…"
    }

    func promptSymlink() {
        guard canMutate, let entry = selectedEntries.first(where: { !$0.isParentNav }) else { return }
        symlinkPrompt = SymlinkPrompt(targetName: entry.name, linkName: "\(entry.name).link")
    }

    func confirmSymlink() {
        guard let prompt = symlinkPrompt,
              let controller
        else { return }
        let linkName = prompt.linkName.trimmingCharacters(in: .whitespacesAndNewlines)
        symlinkPrompt = nil
        guard !linkName.isEmpty, !linkName.contains("/") else { return }
        let linkPath = RemoteFileEntry.join(dir: cwd, name: linkName)
        opInFlight = true
        awaitingSftpResult = true
        refreshAfterOp = cwd
        controller.createSymlink(prompt.targetName, linkPath: linkPath)
        statusMessage = "Creating symlink…"
    }

    func promptDelete() {
        let names = selectedEntries.filter { !$0.isParentNav }.map(\.name)
        guard canMutate, !names.isEmpty else { return }
        deleteConfirmNames = names
    }

    func confirmDelete() {
        let paths = selectedEntries.filter { !$0.isParentNav }.map(\.path)
        deleteConfirmNames = nil
        guard canMutate, !paths.isEmpty else { return }
        pendingDeleteQueue = paths
        deleteNext()
    }

    func copySelectedPath() {
        guard let path = selectedEntries.first(where: { !$0.isParentNav })?.path else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(path, forType: .string)
        statusMessage = "Copied path"
        session?.onStatus?("Copied \(path)", .success)
    }

    func cancelTransfer() {
        controller?.cancelTransfer()
        statusMessage = "Canceling…"
    }

    func resumeTransfer() {
        guard let controller else { return }
        transferInterrupted = false
        isTransferring = true
        awaitingSftpResult = true
        opInFlight = true
        refreshAfterOp = cwd
        transferCurrentName = "Resuming…"
        controller.resumeInterruptedTransfer()
    }

    func discardTransfer() {
        controller?.discardInterruptedTransfer()
        finishTransferUI()
        transferInterrupted = false
        transferResumable = false
        interruptedJob = nil
        statusMessage = "Discarded interrupted transfer"
    }

    func confirmOverwrite(overwrite: Bool) {
        guard let confirm = overwriteConfirm else { return }
        overwriteConfirm = nil
        if !overwrite {
            statusMessage = "Upload canceled"
            return
        }
        performUpload(localPaths: confirm.localPaths, remoteDir: confirm.remoteDir)
    }

    // MARK: - Derived

    var canMutate: Bool {
        session?.state == .connected
            && unavailableReason == nil
            && !cwd.isEmpty
            && !opInFlight
            && !isTransferring
    }

    var selectedEntries: [RemoteFileEntry] {
        entries.filter { selectedPaths.contains($0.path) }
    }

    var selectedDownloadablePaths: [String] {
        selectedEntries.filter { !$0.isParentNav }.map(\.path)
    }

    var showSizeColumn: Bool { ESSAppSettings.shared().showSizeColumn }
    var showPermissionsColumn: Bool { ESSAppSettings.shared().showPermissionsColumn }
    var showModifiedColumn: Bool { ESSAppSettings.shared().showModifiedColumn }

    var transferFraction: Double? {
        guard transferBytesTotal > 0 else { return nil }
        return Double(transferBytesDone) / Double(transferBytesTotal)
    }

    // MARK: - Private

    private func wireCallbacks() {
        guard let controller else { return }

        controller.onDirectoryListed = { [weak self] path, rawEntries in
            Task { @MainActor in
                self?.handleDirectoryListed(path: path, raw: rawEntries)
            }
        }
        controller.onEntryResolved = { [weak self] path, isDir, ok, error in
            Task { @MainActor in
                self?.handleEntryResolved(path: path, isDir: isDir, ok: ok, error: error)
            }
        }
        controller.onPathCanonicalized = { [weak self] requested, canonical in
            Task { @MainActor in
                self?.handlePathCanonicalized(requested: requested, canonical: canonical)
            }
        }
        controller.onSftpFinished = { [weak self] message in
            Task { @MainActor in
                self?.handleSftpFinished(message: message)
            }
        }
        controller.onSftpError = { [weak self] message in
            Task { @MainActor in
                self?.handleSftpError(message: message)
            }
        }
        controller.onSftpCanceled = { [weak self] message in
            Task { @MainActor in
                self?.handleSftpCanceled(message: message)
            }
        }
        controller.onSftpInterrupted = { [weak self] job in
            Task { @MainActor in
                self?.handleSftpInterrupted(job: job)
            }
        }
        controller.onSftpUnavailable = { [weak self] message in
            Task { @MainActor in
                self?.handleSftpUnavailable(message: message)
            }
        }
        controller.onSftpProgress = { [weak self] done, total, name in
            Task { @MainActor in
                self?.handleSftpProgress(done: done, total: total, name: name)
            }
        }
        controller.onTransferResumableChanged = { [weak self] resumable in
            Task { @MainActor in
                self?.transferResumable = resumable
                if !resumable {
                    self?.transferInterrupted = false
                    self?.interruptedJob = nil
                }
            }
        }
        controller.onRemoteFsOpened = { [weak self] backend in
            Task { @MainActor in
                self?.fsBackend = FsBackendKind(rawValue: backend) ?? .none
                if self?.fsBackend == .scp {
                    self?.statusMessage = "Using SCP + shell fallback"
                    self?.session?.onStatus?("Using SCP + shell fallback", .warning)
                }
            }
        }
    }

    private func handleDirectoryListed(path: String, raw: [Any]) {
        isListing = false
        if let pending = pendingNavPath, path != pending, path != cwd {
            // Stale listing for a previous navigation — ignore if we already moved on.
        }
        if pendingNavPath == path {
            pendingNavPath = nil
        }

        var parsed: [RemoteFileEntry] = []
        for item in raw {
            guard let dict = item as? [AnyHashable: Any],
                  let entry = RemoteFileEntry.from(dict: dict)
            else { continue }
            parsed.append(entry)
        }
        allEntries = parsed.sorted { lhs, rhs in
            if lhs.isDir != rhs.isDir { return lhs.isDir && !rhs.isDir }
            return lhs.name.localizedCaseInsensitiveCompare(rhs.name) == .orderedAscending
        }
        applyFilter(for: path)
        if path == cwd || cwd.isEmpty {
            cwd = path
            pathBarText = path
        }
    }

    private func applyFilter(for path: String) {
        let showHidden = ESSAppSettings.shared().showHiddenFiles
        var visible = allEntries.filter { entry in
            if showHidden { return true }
            return !entry.name.hasPrefix(".")
        }
        if path != "/" {
            visible.insert(RemoteFileEntry.parent(of: path), at: 0)
        }
        entries = visible
    }

    func reloadHiddenFilter() {
        guard !cwd.isEmpty else { return }
        applyFilter(for: cwd)
    }

    private func handlePathCanonicalized(requested: String, canonical: String) {
        if let pending = pendingRootRequest, pending == requested || requested == "." {
            pendingRootRequest = nil
            navigateTo(canonical)
            return
        }
        if pendingNavPath == requested {
            navigateTo(canonical)
        }
    }

    private func handleEntryResolved(path: String, isDir: Bool, ok: Bool, error: String) {
        guard pendingResolvePath == path else { return }
        pendingResolvePath = nil
        opInFlight = false
        if !ok {
            statusMessage = error.isEmpty ? "Could not resolve \(path)" : error
            session?.onStatus?(statusMessage, .error)
            return
        }
        if isDir {
            navigateTo(path)
        } else {
            selectedPaths = [path]
            openWithSelected()
        }
    }

    private func handleSftpFinished(message: String) {
        if openWithActive {
            finishCurrentOpenWith()
            return
        }
        if !pendingDeleteQueue.isEmpty {
            deleteNext()
            return
        }
        awaitingSftpResult = false
        opInFlight = false
        finishTransferUI()
        if let refresh = refreshAfterOp {
            refreshAfterOp = nil
            if refresh == cwd {
                refreshCurrentQuiet()
            }
        }
        if !message.isEmpty {
            statusMessage = message
            session?.onStatus?(message, .success)
        }
    }

    private func handleSftpError(message: String) {
        if openWithActive {
            openWithQueue.removeAll()
            openWithActive = false
        }
        pendingDeleteQueue.removeAll()
        awaitingSftpResult = false
        opInFlight = false
        if transferResumable {
            transferInterrupted = true
            isTransferring = false
        } else {
            finishTransferUI()
        }
        statusMessage = message
        session?.onStatus?(message, .error)
        if let prev = navPreviousPath, pendingNavPath != nil {
            pendingNavPath = nil
            cwd = prev
            pathBarText = prev
            refreshCurrentQuiet()
        }
    }

    private func handleSftpCanceled(message: String) {
        if openWithActive {
            openWithQueue.removeAll()
            openWithActive = false
        }
        awaitingSftpResult = false
        opInFlight = false
        if transferResumable {
            transferInterrupted = true
            isTransferring = false
        } else {
            finishTransferUI()
        }
        statusMessage = message.isEmpty ? "Canceled" : message
    }

    private func handleSftpInterrupted(job: [AnyHashable: Any]) {
        interruptedJob = TransferJobInfo.from(dict: job)
        transferInterrupted = true
        transferResumable = true
        isTransferring = false
        awaitingSftpResult = false
        opInFlight = false
        transferBytesDone = interruptedJob?.bytesDone ?? 0
        transferBytesTotal = interruptedJob?.bytesTotal ?? 0
        transferCurrentName = interruptedJob?.summary ?? "Interrupted"
        statusMessage = "Transfer interrupted"
    }

    private func handleSftpUnavailable(message: String) {
        unavailableReason = message
        isListing = false
        pathBarText = "Files unavailable"
        entries = []
        allEntries = []
        statusMessage = message
        session?.onStatus?(message, .warning)
    }

    private func handleSftpProgress(done: Int64, total: Int64, name: String) {
        isTransferring = true
        transferInterrupted = false
        transferBytesDone = done
        transferBytesTotal = total
        transferCurrentName = name
    }

    private func beginUpload(localPaths: [String]) {
        guard !localPaths.isEmpty else { return }
        let remoteDir = targetDirectory()
        guard !remoteDir.isEmpty else { return }

        if remoteDir == cwd {
            let conflicts = localPaths.compactMap { path -> String? in
                let name = (path as NSString).lastPathComponent
                return allEntries.contains(where: { $0.name == name }) ? name : nil
            }
            if !conflicts.isEmpty {
                overwriteConfirm = OverwriteConfirm(
                    conflicts: conflicts,
                    localPaths: localPaths,
                    remoteDir: remoteDir
                )
                return
            }
        }
        performUpload(localPaths: localPaths, remoteDir: remoteDir)
    }

    private func performUpload(localPaths: [String], remoteDir: String) {
        guard let controller else { return }
        opInFlight = true
        awaitingSftpResult = true
        isTransferring = true
        transferCurrentName = "Uploading…"
        transferBytesDone = 0
        transferBytesTotal = 0
        refreshAfterOp = cwd
        controller.uploadFiles(localPaths, remoteDir: remoteDir)
        statusMessage = "Uploading…"
    }

    private func startDownload(remotePaths: [String], localDir: String) {
        guard let controller, !remotePaths.isEmpty else { return }
        opInFlight = true
        awaitingSftpResult = true
        isTransferring = true
        transferCurrentName = "Downloading…"
        transferBytesDone = 0
        transferBytesTotal = 0
        controller.downloadPaths(remotePaths, localDir: localDir)
        statusMessage = "Downloading…"
    }

    private func startNextOpenWith() {
        guard let controller, let next = openWithQueue.first else {
            openWithActive = false
            finishTransferUI()
            return
        }
        opInFlight = true
        awaitingSftpResult = true
        isTransferring = true
        transferCurrentName = next.fileName
        transferBytesDone = 0
        transferBytesTotal = 0
        controller.downloadPaths([next.remotePath], localDir: next.localDir, followSymlinks: true)
        statusMessage = "Opening \(next.fileName)…"
    }

    private func finishCurrentOpenWith() {
        guard let item = openWithQueue.first else {
            openWithActive = false
            finishTransferUI()
            return
        }
        openWithQueue.removeFirst()
        let localPath = (item.localDir as NSString).appendingPathComponent(item.fileName)
        let url = URL(fileURLWithPath: localPath)
        if FileManager.default.fileExists(atPath: localPath) {
            NSWorkspace.shared.open(url)
        } else if let found = try? FileManager.default.contentsOfDirectory(atPath: item.localDir).first {
            NSWorkspace.shared.open(URL(fileURLWithPath: (item.localDir as NSString).appendingPathComponent(found)))
        }
        if openWithQueue.isEmpty {
            openWithActive = false
            awaitingSftpResult = false
            opInFlight = false
            finishTransferUI()
            statusMessage = "Opened"
        } else {
            startNextOpenWith()
        }
    }

    private func deleteNext() {
        guard let controller else {
            pendingDeleteQueue.removeAll()
            return
        }
        guard let next = pendingDeleteQueue.first else {
            awaitingSftpResult = false
            opInFlight = false
            refreshCurrentQuiet()
            statusMessage = "Deleted"
            return
        }
        pendingDeleteQueue.removeFirst()
        opInFlight = true
        awaitingSftpResult = true
        refreshAfterOp = cwd
        controller.removePath(next, recursive: true)
        statusMessage = "Deleting…"
    }

    private func targetDirectory() -> String {
        if let selected = selectedEntries.first(where: { !$0.isParentNav && $0.isNavigableDirectory }) {
            return selected.path
        }
        if let selected = selectedEntries.first(where: { !$0.isParentNav && !$0.isDir }) {
            return RemoteFileEntry.parentPath(of: selected.path)
        }
        return cwd
    }

    private func refreshCurrentQuiet() {
        guard !cwd.isEmpty, let controller else { return }
        isListing = true
        controller.listDirectory(cwd)
    }

    private func finishTransferUI() {
        isTransferring = false
        transferBytesDone = 0
        transferBytesTotal = 0
        transferCurrentName = ""
    }

    private func clearListing() {
        cwd = ""
        pathBarText = ""
        entries = []
        allEntries = []
        selectedPaths = []
        isListing = false
        unavailableReason = nil
        finishTransferUI()
        opInFlight = false
    }
}
