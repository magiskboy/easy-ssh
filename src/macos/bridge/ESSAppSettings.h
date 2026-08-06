/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Thin ObjC wrapper over AppSettings. Fonts are family + point size (no QFont).
/// onSettingsChanged (if set) is invoked on the main queue.
@interface ESSAppSettings : NSObject

@property (nonatomic, copy, nullable) void (^onSettingsChanged)(void);

+ (ESSAppSettings *)sharedSettings;

// --- File Explorer ---
@property (nonatomic, assign) BOOL showSizeColumn;
@property (nonatomic, assign) BOOL showPermissionsColumn;
@property (nonatomic, assign) BOOL showModifiedColumn;
@property (nonatomic, assign) BOOL showHiddenFiles;
@property (nonatomic, copy) NSString *defaultDownloadDir;

// --- Terminal ---
@property (nonatomic, copy) NSString *terminalFontFamily;
@property (nonatomic, assign) double terminalFontPointSize;
@property (nonatomic, copy) NSString *colorScheme;
@property (nonatomic, assign) NSInteger historySize;
@property (nonatomic, assign) NSInteger cursorShape;
@property (nonatomic, assign) BOOL cursorBlink;
@property (nonatomic, assign) BOOL confirmMultilinePaste;
@property (nonatomic, assign) BOOL smartLayout;

// --- Session / tray ---
@property (nonatomic, assign) BOOL autoReconnect;
@property (nonatomic, assign) BOOL restoreWorkspace;
@property (nonatomic, assign) BOOL closeToTray;
@property (nonatomic, assign) BOOL minimizeToTray;
@property (nonatomic, assign) BOOL startInTray;
@property (nonatomic, assign) BOOL trayNotifications;
@property (nonatomic, assign) BOOL trayMinimizeHintShown;

// --- Transfers ---
@property (nonatomic, assign) NSInteger transferStallTimeoutSec;
@property (nonatomic, assign) BOOL autoResumeTransferAfterReconnect;

// --- Appearance ---
@property (nonatomic, copy) NSString *uiFontMode;
@property (nonatomic, copy) NSString *uiFontFamily;
@property (nonatomic, assign) double uiFontPointSize;
@property (nonatomic, copy) NSString *themeId;
@property (nonatomic, copy) NSString *customThemePath;

// --- Window / sidebar ---
@property (nonatomic, copy, nullable) NSData *windowGeometry;
@property (nonatomic, assign) NSInteger sidebarWidth;
@property (nonatomic, copy) NSString *sidebarTabId;

// --- Workspace blob (prefer ESSWorkspaceStore for structured access) ---
@property (nonatomic, copy, nullable) NSData *workspaceStateData;

// --- Recent ---
- (NSArray<NSUUID *> *)recentConnectionIdsWithLimit:(NSInteger)limit;
- (void)recordRecentConnection:(NSUUID *)connectionId;

// --- Shortcuts (key sequences as portable strings, e.g. "Ctrl+N" / "Meta+N") ---
- (nullable NSString *)shortcutForActionId:(NSString *)actionId;
- (void)setShortcut:(NSString *)sequence forActionId:(NSString *)actionId;
- (void)resetShortcutsToDefaults;
+ (NSArray<NSString *> *)shortcutActionIds;
+ (NSString *)defaultShortcutForActionId:(NSString *)actionId;
+ (NSString *)shortcutLabelForActionId:(NSString *)actionId;
+ (NSString *)shortcutGroupForActionId:(NSString *)actionId;

- (void)notifyChanged;

@end

NS_ASSUME_NONNULL_END
