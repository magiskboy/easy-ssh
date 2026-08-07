/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef ESS_TUNNEL_STORE_H
#define ESS_TUNNEL_STORE_H

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Stable tunnel dictionary keys mirror core TunnelStore persistence:
///   id, connectionId (NSUUID)
///   name (NSString), type (NSString: local|remote|dynamic), enabled (NSNumber bool)
///   localKind, remoteKind (NSString: tcp|unix)
///   localHost, localPort, localSocketPath
///   remoteHost, remotePort, remoteSocketPath
///   socksAuth (NSString: none|usernamePassword), socksUsername
/// Runtime-only socksPassword is intentionally excluded from persistence.
@interface ESSTunnelStore : NSObject

+ (NSArray<NSDictionary *> *)loadAll;
+ (NSArray<NSDictionary *> *)loadForConnectionId:(NSUUID *)connectionId;
+ (void)saveAll:(NSArray<NSDictionary *> *)tunnels;
+ (void)removeByConnectionId:(NSUUID *)connectionId;

@end

NS_ASSUME_NONNULL_END

#endif
