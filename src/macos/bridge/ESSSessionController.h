/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#import <Foundation/Foundation.h>

#import "ESSConnectionStore.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ESSHostKeyPromptReason) {
    ESSHostKeyPromptReasonUnknown = 0,
    ESSHostKeyPromptReasonChanged = 1,
    ESSHostKeyPromptReasonOther = 2,
};

/// One SSH transport + shells/SFTP/tunnels. Mirrors SshWorker for future Swift UI features.
///
/// Threading: all callback blocks are invoked on the main queue.
@interface ESSSessionController : NSObject

@property (nonatomic, copy, nullable) void (^onConnected)(NSUUID *shellId);
@property (nonatomic, copy, nullable) void (^onData)(NSUUID *shellId, NSData *data);
@property (nonatomic, copy, nullable) void (^onShellOpened)(NSUUID *shellId);
@property (nonatomic, copy, nullable) void (^onShellClosed)(NSUUID *shellId);
@property (nonatomic, copy, nullable) void (^onShellFailed)(NSUUID *shellId, NSString *message);
@property (nonatomic, copy, nullable) void (^onHostKeyPrompt)(ESSHostKeyPromptReason reason,
                                                              NSString *fingerprint,
                                                              NSString *contextLabel);
@property (nonatomic, copy, nullable) void (^onError)(NSString *message);
@property (nonatomic, copy, nullable) void (^onDisconnected)(void);
@property (nonatomic, copy, nullable) void (^onAgentForwardingWarning)(NSString *message);

/// SFTP / remote FS entry dictionaries (onDirectoryListed) use stable keys:
///   name (NSString), path (NSString), isDir (NSNumber bool), isSymlink (NSNumber bool),
///   linkIsDir (NSNumber bool), linkTarget (NSString), size (NSNumber),
///   permissions (NSString), mtime (NSNumber unix seconds).
///
/// TransferJob dictionaries (onSftpInterrupted) use stable keys:
///   direction (NSNumber: 0=upload, 1=download), localPath (NSString),
///   remoteFinalPath (NSString), filepartPath (NSString),
///   bytesDone (NSNumber), bytesTotal (NSNumber), backend (NSNumber FsBackend),
///   lastMessage (NSString).
///
/// Explorer row keys (Phase 6 — documented only; not emitted yet):
///   Process: pid, ppid, uid, user, cpuPercent, memPercent, stateCode, nice, priority,
///            elapsedSeconds, cpuTime, rssKiB, vszKiB, comm, command
///   Container: runtime, containerId, name, image, state, pid, runtimeNamespace,
///              cpuPercent, memPercent, memUsage
///   Service: manager, unit, description, loadState, activeState, subState, unitFileState, mainPid
///   SystemInfo: structured sections from SystemInfoParser (cpu/mem/disk/gpu widgets)
@property (nonatomic, copy, nullable) void (^onDirectoryListed)(NSString *path, NSArray<NSDictionary *> *entries);
@property (nonatomic, copy, nullable) void (^onEntryResolved)(NSString *path, BOOL isDir, BOOL ok, NSString *error);
@property (nonatomic, copy, nullable) void (^onPathCanonicalized)(NSString *requested, NSString *canonical);
@property (nonatomic, copy, nullable) void (^onSftpFinished)(NSString *message);
@property (nonatomic, copy, nullable) void (^onSftpError)(NSString *message);
@property (nonatomic, copy, nullable) void (^onSftpCanceled)(NSString *message);
@property (nonatomic, copy, nullable) void (^onSftpInterrupted)(NSDictionary *job);
@property (nonatomic, copy, nullable) void (^onSftpUnavailable)(NSString *message);
@property (nonatomic, copy, nullable) void (^onSftpProgress)(int64_t bytesDone, int64_t bytesTotal, NSString *currentName);
@property (nonatomic, copy, nullable) void (^onTransferResumableChanged)(BOOL resumable);
@property (nonatomic, copy, nullable) void (^onRemoteFsOpened)(NSInteger backend);

/// Tunnels (local TCP now; remote/dynamic/UDS in Phase 10).
@property (nonatomic, copy, nullable) void (^onTunnelStatusChanged)(NSUUID *tunnelId, NSString *status, NSString *detail);
@property (nonatomic, copy, nullable) void (^onTunnelError)(NSUUID *tunnelId, NSString *message);

@property (nonatomic, copy, nullable) void (^onCommandFinished)(NSString *requestId,
                                                                NSInteger exitStatus,
                                                                NSData *stdoutData,
                                                                NSData *stderrData,
                                                                NSString *errorMessage);

@property (nonatomic, readonly, nullable) NSUUID *primaryShellId;
@property (nonatomic, readonly, getter=isConnected) BOOL connected;

/// Full connection payload (preferred).
- (void)connectWithConnection:(ESSConnectionInfo *)connection
                  credentials:(nullable ESSSessionCredentials *)credentials
                         cols:(NSInteger)cols
                         rows:(NSInteger)rows;

/// Quick-connect convenience; builds a minimal ESSConnectionInfo.
- (void)connectWithHost:(NSString *)host
                   port:(NSInteger)port
               username:(NSString *)username
               authType:(ESSAuthType)authType
               password:(nullable NSString *)password
         privateKeyPath:(nullable NSString *)privateKeyPath
                   cols:(NSInteger)cols
                   rows:(NSInteger)rows;

- (void)disconnect;
- (void)reconnectWithCols:(NSInteger)cols rows:(NSInteger)rows;
- (void)respondHostKeyTrust:(BOOL)accept;

- (void)writeData:(NSData *)data shellId:(nullable NSUUID *)shellId;
- (void)resizeCols:(NSInteger)cols rows:(NSInteger)rows shellId:(nullable NSUUID *)shellId;
- (void)openShell:(NSUUID *)shellId cols:(NSInteger)cols rows:(NSInteger)rows;
- (void)closeShell:(NSUUID *)shellId;

- (void)listDirectory:(NSString *)path;
- (void)createDirectory:(NSString *)path;
- (void)createSymlink:(NSString *)target linkPath:(NSString *)linkPath;
- (void)resolveEntry:(NSString *)path;
- (void)renamePath:(NSString *)from to:(NSString *)to;
- (void)removePath:(NSString *)path recursive:(BOOL)recursive;
- (void)uploadFiles:(NSArray<NSString *> *)localPaths remoteDir:(NSString *)remoteDir;
- (void)uploadFileTo:(NSString *)localPath remotePath:(NSString *)remotePath;
- (void)downloadPaths:(NSArray<NSString *> *)remotePaths localDir:(NSString *)localDir;
- (void)downloadPaths:(NSArray<NSString *> *)remotePaths
             localDir:(NSString *)localDir
       followSymlinks:(BOOL)followSymlinks;
- (void)canonicalizePath:(NSString *)path;
- (void)cancelTransfer;
- (void)resumeInterruptedTransfer;
- (void)discardInterruptedTransfer;

- (void)startLocalTunnelNamed:(NSString *)name
                    localHost:(NSString *)localHost
                    localPort:(NSInteger)localPort
                   remoteHost:(NSString *)remoteHost
                   remotePort:(NSInteger)remotePort;
- (void)stopTunnel:(NSUUID *)tunnelId;
- (void)stopAllTunnels;

- (void)execCommand:(NSString *)command requestId:(NSString *)requestId;

@end

NS_ASSUME_NONNULL_END
