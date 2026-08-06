/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#import <Foundation/Foundation.h>

#import "ESSConnectionStore.h"

NS_ASSUME_NONNULL_BEGIN

@interface ESSSshConfigHost : NSObject
@property (nonatomic, copy) NSString *aliasName;
@property (nonatomic, copy) NSString *hostName;
@property (nonatomic, copy) NSString *user;
@property (nonatomic, assign) NSInteger port;
@property (nonatomic, copy) NSArray<NSString *> *identityFiles;
@property (nonatomic, copy) NSString *proxyJump;
@property (nonatomic, copy) NSString *proxyCommand;
@property (nonatomic, assign) BOOL forwardAgent;
@end

@interface ESSSshConfigParser : NSObject

+ (NSString *)defaultConfigPath;

+ (NSArray<ESSSshConfigHost *> *)loadHostsFromPath:(nullable NSString *)path;

/// Materialize Connection rows (source=SshConfig). Does not merge into ESSConnectionStore.
+ (NSArray<ESSConnectionInfo *> *)connectionsFromConfigPath:(nullable NSString *)path;

@end

NS_ASSUME_NONNULL_END
