// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

import Combine
import Foundation
import SwiftUI

enum SettingsTab: String, CaseIterable, Identifiable {
    case general
    case fileExplorer
    case shell
    case shortcuts

    var id: String { rawValue }

    var title: String {
        switch self {
        case .general: return "General"
        case .fileExplorer: return "File Explorer"
        case .shell: return "Shell"
        case .shortcuts: return "Shortcuts"
        }
    }
}

@MainActor
final class SettingsModel: ObservableObject {
    @Published var selectedTab: SettingsTab = .general

    // General — appearance
    @Published var uiFontMode: String = "system"
    @Published var uiFontFamily: String = ""
    @Published var uiFontPointSize: Double = 13
    @Published var themeId: String = "system"

    // General — session
    @Published var autoReconnect = false
    @Published var restoreWorkspace = true

    // General — tray
    @Published var closeToTray = true
    @Published var minimizeToTray = false
    @Published var startInTray = false
    @Published var trayNotifications = true

    // General — transfers
    @Published var transferStallTimeoutSec: Int = 60
    @Published var autoResumeTransferAfterReconnect = true

    // File explorer
    @Published var showSizeColumn = true
    @Published var showPermissionsColumn = true
    @Published var showModifiedColumn = false
    @Published var showHiddenFiles = false
    @Published var defaultDownloadDir: String = ""

    // Shell
    @Published var terminalFontFamily: String = ""
    @Published var terminalFontPointSize: Double = 13
    @Published var colorScheme: String = "WhiteOnBlack"
    @Published var historySize: Int = 10_000
    @Published var cursorShape: Int = 0
    @Published var cursorBlink = true
    @Published var confirmMultilinePaste = true
    @Published var smartLayout = true

    /// actionId → portable key sequence
    @Published var shortcutDraft: [String: String] = [:]

    func reloadDraftFromStore() {
        let s = ESSAppSettings.shared()

        uiFontMode = s.uiFontMode
        uiFontFamily = s.uiFontFamily
        uiFontPointSize = s.uiFontPointSize
        themeId = AppAppearance.normalizedThemeId(s.themeId)

        autoReconnect = s.autoReconnect
        restoreWorkspace = s.restoreWorkspace
        closeToTray = s.closeToTray
        minimizeToTray = s.minimizeToTray
        startInTray = s.startInTray
        trayNotifications = s.trayNotifications
        transferStallTimeoutSec = Int(s.transferStallTimeoutSec)
        autoResumeTransferAfterReconnect = s.autoResumeTransferAfterReconnect

        showSizeColumn = s.showSizeColumn
        showPermissionsColumn = s.showPermissionsColumn
        showModifiedColumn = s.showModifiedColumn
        showHiddenFiles = s.showHiddenFiles
        defaultDownloadDir = s.defaultDownloadDir

        terminalFontFamily = s.terminalFontFamily
        terminalFontPointSize = s.terminalFontPointSize
        colorScheme = s.colorScheme.isEmpty ? "WhiteOnBlack" : s.colorScheme
        historySize = Int(s.historySize)
        cursorShape = Int(s.cursorShape)
        cursorBlink = s.cursorBlink
        confirmMultilinePaste = s.confirmMultilinePaste
        smartLayout = s.smartLayout

        var shortcuts: [String: String] = [:]
        for actionId in ESSAppSettings.shortcutActionIds() {
            let id = actionId as String
            shortcuts[id] = s.shortcut(forActionId: id) ?? ""
        }
        shortcutDraft = shortcuts
    }

    func applyToStore() {
        let s = ESSAppSettings.shared()

        s.uiFontMode = uiFontMode
        s.uiFontFamily = uiFontFamily
        s.uiFontPointSize = uiFontPointSize
        s.themeId = themeId

        s.autoReconnect = autoReconnect
        s.restoreWorkspace = restoreWorkspace
        s.closeToTray = closeToTray
        s.minimizeToTray = minimizeToTray
        s.startInTray = startInTray
        s.trayNotifications = trayNotifications
        s.transferStallTimeoutSec = transferStallTimeoutSec
        s.autoResumeTransferAfterReconnect = autoResumeTransferAfterReconnect

        s.showSizeColumn = showSizeColumn
        s.showPermissionsColumn = showPermissionsColumn
        s.showModifiedColumn = showModifiedColumn
        s.showHiddenFiles = showHiddenFiles
        s.defaultDownloadDir = defaultDownloadDir

        s.terminalFontFamily = terminalFontFamily
        s.terminalFontPointSize = terminalFontPointSize
        s.colorScheme = colorScheme
        s.historySize = historySize
        s.cursorShape = cursorShape
        s.cursorBlink = cursorBlink
        s.confirmMultilinePaste = confirmMultilinePaste
        s.smartLayout = smartLayout

        for (actionId, sequence) in shortcutDraft {
            s.setShortcut(sequence, forActionId: actionId)
        }

        s.notifyChanged()
    }

    func resetShortcutsInDraftToDefaults() {
        var shortcuts: [String: String] = [:]
        for actionId in ESSAppSettings.shortcutActionIds() {
            let id = actionId as String
            shortcuts[id] = ESSAppSettings.defaultShortcut(forActionId: id)
        }
        shortcutDraft = shortcuts
    }

    func cancelEdits() {
        reloadDraftFromStore()
    }
}
