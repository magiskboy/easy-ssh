/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ESSAuthType) {
    ESSAuthTypePassword = 0,
    ESSAuthTypePrivateKey = 1,
};

typedef NS_ENUM(NSInteger, ESSConnectionSource) {
    ESSConnectionSourceApp = 0,
    ESSConnectionSourceSshConfig = 1,
};

typedef NS_ENUM(NSInteger, ESSProxyMode) {
    ESSProxyModeNone = 0,
    ESSProxyModeProxyJump = 1,
    ESSProxyModeProxyCommand = 2,
};

@interface ESSJumpHop : NSObject
@property (nonatomic, copy) NSString *host;
@property (nonatomic, assign) NSInteger port;
@property (nonatomic, copy) NSString *username;
@property (nonatomic, assign) ESSAuthType authType;
@property (nonatomic, copy, nullable) NSString *privateKeyPath;
@property (nonatomic, assign) BOOL useTargetCredentials;
@end

/// WinSCP-style shell + command templates for SCP remote FS fallback.
@interface ESSShellCommandSet : NSObject
@property (nonatomic, copy) NSString *shell;
@property (nonatomic, copy) NSString *listingCommand;
@property (nonatomic, copy) NSString *listFileCommand;
@property (nonatomic, copy) NSString *mkdirCommand;
@property (nonatomic, copy) NSString *removeCommand;
@property (nonatomic, copy) NSString *renameCommand;
@property (nonatomic, copy) NSString *pwdCommand;
@property (nonatomic, copy) NSString *realpathCommand;
@property (nonatomic, copy) NSString *symlinkCommand;
@property (nonatomic, copy) NSString *readlinkCommand;
@property (nonatomic, assign) BOOL clearAliases;
@property (nonatomic, assign) BOOL clearNationalVars;
@property (nonatomic, assign) BOOL tryFullTime;
@property (nonatomic, assign) BOOL ignoreLsWarnings;
@property (nonatomic, assign) BOOL allowScpFallback;
@end

/// Full connection payload mirroring core `Connection`.
@interface ESSConnectionInfo : NSObject
@property (nonatomic, copy) NSUUID *connectionId;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *host;
@property (nonatomic, assign) NSInteger port;
@property (nonatomic, copy) NSString *username;
@property (nonatomic, assign) ESSAuthType authType;
@property (nonatomic, assign) BOOL savePassword;
@property (nonatomic, copy, nullable) NSString *privateKeyPath;
@property (nonatomic, copy) NSString *startupDirectory;
@property (nonatomic, assign) ESSConnectionSource source;
@property (nonatomic, copy) NSString *configAlias;
@property (nonatomic, assign) ESSProxyMode proxyMode;
@property (nonatomic, copy) NSArray<ESSJumpHop *> *jumpHops;
@property (nonatomic, copy) NSString *proxyCommand;
@property (nonatomic, assign) NSInteger keepAliveIntervalSec;
@property (nonatomic, assign) NSInteger keepAliveCountMax;
@property (nonatomic, assign) BOOL compressionEnabled;
@property (nonatomic, assign) BOOL agentForwarding;
@property (nonatomic, strong) ESSShellCommandSet *shellCommands;
@property (nonatomic, copy) NSString *displayText;
@end

/// Secrets passed at connect time (not persisted on Connection).
@interface ESSSessionCredentials : NSObject
@property (nonatomic, copy, nullable) NSString *targetSecret;
@property (nonatomic, copy, nullable) NSString *gatewaySecret;
@end

@interface ESSConnectionStore : NSObject

+ (NSArray<ESSConnectionInfo *> *)loadConnections;
+ (BOOL)saveConnections:(NSArray<ESSConnectionInfo *> *)connections
                  error:(NSError *_Nullable *_Nullable)error;

@end

NS_ASSUME_NONNULL_END
