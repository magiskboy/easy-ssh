/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef ESS_SESSION_CONTROLLER_H
#define ESS_SESSION_CONTROLLER_H

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
/// Explorer row keys (Phase 6):
///   Process: pid, ppid, uid, user, cpuPercent, memPercent, stateCode, nice, priority,
///            elapsedSeconds, cpuTime, rssKiB, vszKiB, comm, command
///   Container: runtime, containerId, name, image, state, pid, runtimeNamespace,
///              cpuPercent, memPercent, memUsage
///   Service: manager, unit, description, loadState, activeState, subState, unitFileState, mainPid
///   SystemInfo: nested dictionaries (os, load, cpu, mem, disks, diskIo, nics, temps, gpus,
///               gpuProcesses, virt)
///
/// Explorer kinds for start/stop/refresh: @"process" | @"container" | @"service" | @"systemInfo"
///
/// Capability values (onExplorerCapability): @"checking" | @"available" | @"unavailable" |
///   @"permissionDenied" | @"error"
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

/// Tunnel dictionaries use the same stable keys as ESSTunnelStore.
@property (nonatomic, copy, nullable) void (^onTunnelStatusChanged)(NSUUID *tunnelId, NSString *status, NSString *detail);
@property (nonatomic, copy, nullable) void (^onTunnelError)(NSUUID *tunnelId, NSString *message);

@property (nonatomic, copy, nullable) void (^onCommandFinished)(NSString *requestId,
                                                                NSInteger exitStatus,
                                                                NSData *stdoutData,
                                                                NSData *stderrData,
                                                                NSString *errorMessage);

/// Explorers (Phase 6). Kind is process|container|service|systemInfo.
@property (nonatomic, copy, nullable) void (^onExplorerCapability)(NSString *kind,
                                                                   NSString *capability,
                                                                   NSString *message);
@property (nonatomic, copy, nullable) void (^onExplorerBusy)(NSString *kind, BOOL busy);
@property (nonatomic, copy, nullable) void (^onExplorerFailed)(NSString *kind, NSString *message);
@property (nonatomic, copy, nullable) void (^onProcessSnapshot)(NSArray<NSDictionary *> *rows);
@property (nonatomic, copy, nullable) void (^onContainerSnapshot)(NSArray<NSDictionary *> *rows);
@property (nonatomic, copy, nullable) void (^onServiceSnapshot)(NSArray<NSDictionary *> *rows);
@property (nonatomic, copy, nullable) void (^onSystemInfoSnapshot)(NSDictionary *snapshot);
@property (nonatomic, copy, nullable) void (^onContainerInspect)(NSDictionary *info, NSString * _Nullable error);
@property (nonatomic, copy, nullable) void (^onServiceInspect)(NSDictionary *info, NSString * _Nullable error);

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

- (void)startTunnel:(NSDictionary *)definition;
- (NSString *)validationErrorForTunnel:(NSDictionary *)definition;
- (void)stopTunnel:(NSUUID *)tunnelId;
- (void)stopAllTunnels;

- (void)execCommand:(NSString *)command requestId:(NSString *)requestId;

- (void)startExplorer:(NSString *)kind;
- (void)stopExplorer:(NSString *)kind;
- (void)refreshExplorer:(NSString *)kind;
- (void)stopAllExplorers;

/// Inspect uses seed fields from a list row dictionary.
- (void)inspectContainer:(NSDictionary *)seed;
- (void)inspectService:(NSDictionary *)seed;

/// Format last SystemInfo snapshot (empty string if none).
- (NSString *)systemInfoText;
- (NSString *)systemInfoJson;

/// journalctl follow command for a service row (empty if invalid).
- (NSString *)serviceFollowLogsCommand:(NSDictionary *)seed lines:(NSInteger)lines;

@end

NS_ASSUME_NONNULL_END

#endif
