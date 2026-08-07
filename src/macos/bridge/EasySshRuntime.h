/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef EASY_SSH_RUNTIME_H
#define EASY_SSH_RUNTIME_H

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Owns the process-wide QCoreApplication. Call +start before any other bridge API.
/// Does not call QCoreApplication::exec — Qt rides the NS/CF run loop.
///
/// Phase 0 smoke checklist (manual):
/// 1. ESSConnectionStore load/save round-trip preserves proxy + shellCommands + source/configAlias
/// 2. ESSSecretStore store then read password for a UUID (keychain / qt6keychain)
/// 3. ESSSessionController connectWithConnection:credentials: compiles; reconnect keeps full Connection
/// 4. ESSAppSettings / ESSWorkspaceStore.loadState / ESSSshConfigParser callable from Swift
@interface EasySshRuntime : NSObject

+ (void)start;
+ (BOOL)isRunning;
+ (void)shutdown;

/// Absolute path of the process log file (see core Logging::logFilePath).
+ (NSString *)logFilePath;

@end

NS_ASSUME_NONNULL_END

#endif
