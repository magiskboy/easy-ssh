/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef ESS_WORKSPACE_STORE_H
#define ESS_WORKSPACE_STORE_H

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ESSWorkspaceShellEntry : NSObject
@property (nonatomic, copy) NSUUID *shellId;
@property (nonatomic, copy) NSString *title;
@end

@interface ESSWorkspaceSessionEntry : NSObject
@property (nonatomic, copy) NSUUID *connectionId;
@property (nonatomic, copy, nullable) NSUUID *activeShellId;
@property (nonatomic, copy) NSString *activeToolId;
@property (nonatomic, copy) NSArray<ESSWorkspaceShellEntry *> *shells;
@property (nonatomic, copy) NSArray<NSString *> *tools;
@property (nonatomic, copy, nullable) NSData *dockState;
@end

@interface ESSWorkspaceState : NSObject
@property (nonatomic, assign) NSInteger version;
@property (nonatomic, copy, nullable) NSUUID *activeConnectionId;
@property (nonatomic, copy) NSArray<ESSWorkspaceSessionEntry *> *sessions;
@property (nonatomic, readonly, getter=isEmpty) BOOL empty;
@end

@interface ESSWorkspaceStore : NSObject

/// Named loadState (not +load) — ObjC +load is a dyld/runtime hook.
+ (ESSWorkspaceState *)loadState;
+ (BOOL)save:(ESSWorkspaceState *)state error:(NSError *_Nullable *_Nullable)error;

@end

NS_ASSUME_NONNULL_END

#endif
