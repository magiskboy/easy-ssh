// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Foundation

@MainActor
final class WorkspaceCoordinator {
    weak var appModel: AppModel?
    private var restoreQueue: [ESSWorkspaceSessionEntry] = []
    private var restoreActiveConnectionId: UUID?
    private(set) var isRestoring = false
    private var saveWorkItem: DispatchWorkItem?

    init(appModel: AppModel? = nil) {
        self.appModel = appModel
    }

    func beginRestoreIfNeeded() {
        guard let appModel else { return }
        guard ESSAppSettings.shared().restoreWorkspace else { return }

        let state = ESSWorkspaceStore.loadState()
        guard !state.isEmpty else { return }

        restoreActiveConnectionId = state.activeConnectionId as UUID?
        restoreQueue = []
        for entry in state.sessions {
            guard let connectionId = entry.connectionId as UUID? else { continue }
            if appModel.library.connection(id: connectionId) == nil {
                continue
            }
            restoreQueue.append(entry)
        }

        guard !restoreQueue.isEmpty else { return }
        isRestoring = true
        advanceRestore()
    }

    func scheduleSave() {
        guard !isRestoring else { return }
        saveWorkItem?.cancel()
        let work = DispatchWorkItem { [weak self] in
            self?.saveNow()
        }
        saveWorkItem = work
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5, execute: work)
    }

    func saveNow() {
        guard let appModel else { return }
        guard !isRestoring else { return }

        let state = ESSWorkspaceState()
        state.version = 1
        if let selected = appModel.selectedSession {
            state.activeConnectionId = selected.connection.connectionId as UUID
        } else if let last = appModel.sessions.last {
            state.activeConnectionId = last.connection.connectionId as UUID
        }

        var entries: [ESSWorkspaceSessionEntry] = []
        for session in appModel.sessions {
            entries.append(session.captureWorkspaceEntry())
        }
        state.sessions = entries

        do {
            try ESSWorkspaceStore.save(state)
        } catch {
            appModel.status.post("Could not save workspace: \(error.localizedDescription)", level: .warning)
        }
    }

    private func advanceRestore() {
        guard let appModel else { return }
        guard !restoreQueue.isEmpty else {
            finishRestore()
            return
        }

        let entry = restoreQueue.removeFirst()
        guard let connectionId = entry.connectionId as UUID? else {
            advanceRestore()
            return
        }

        appModel.openSessionForRestore(connectionId: connectionId, entry: entry) { [weak self] session in
            guard let self else { return }
            guard let session else {
                self.advanceRestore()
                return
            }
            session.onWorkspaceRestoreFinished = { [weak self] in
                session.onWorkspaceRestoreFinished = nil
                self?.advanceRestore()
            }
        }
    }

    private func finishRestore() {
        guard let appModel else { return }
        if let restoreActiveConnectionId,
           let session = appModel.session(forConnectionId: restoreActiveConnectionId)
        {
            appModel.selectedSessionId = session.id
            appModel.sidebarMode = .sessions
        }
        isRestoring = false
        restoreActiveConnectionId = nil
        scheduleSave()
    }
}
