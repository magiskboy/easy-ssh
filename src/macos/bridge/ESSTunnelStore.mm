/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSTunnelStore.h"
#import "EasySshRuntime.h"

#include "core/tunnel/TunnelStore.h"

#include <QUuid>

namespace {

NSString *qToNS(const QString &s)
{
    return [[NSString alloc] initWithUTF8String:s.toUtf8().constData()];
}

QString nsToQ(NSString *s)
{
    if (s == nil) {
        return {};
    }
    return QString::fromUtf8([s UTF8String]);
}

NSUUID *uuidToNS(const QUuid &id)
{
    if (id.isNull()) {
        return nil;
    }
    return [[NSUUID alloc] initWithUUIDString:qToNS(id.toString(QUuid::WithoutBraces))];
}

QUuid nsToUuid(NSUUID *id)
{
    if (id == nil) {
        return {};
    }
    return QUuid(nsToQ(id.UUIDString));
}

NSString *tunnelTypeToNS(TunnelType type)
{
    switch (type) {
    case TunnelType::Remote:
        return @"remote";
    case TunnelType::Dynamic:
        return @"dynamic";
    case TunnelType::Local:
        return @"local";
    }
    return @"local";
}

TunnelType tunnelTypeFromNS(NSString *value)
{
    if ([value isEqualToString:@"remote"]) {
        return TunnelType::Remote;
    }
    if ([value isEqualToString:@"dynamic"]) {
        return TunnelType::Dynamic;
    }
    return TunnelType::Local;
}

NSString *endpointKindToNS(TunnelEndpointKind kind)
{
    switch (kind) {
    case TunnelEndpointKind::UnixSocket:
        return @"unix";
    case TunnelEndpointKind::Tcp:
        return @"tcp";
    }
    return @"tcp";
}

TunnelEndpointKind endpointKindFromNS(NSString *value)
{
    if ([value isEqualToString:@"unix"] || [value isEqualToString:@"unixSocket"]) {
        return TunnelEndpointKind::UnixSocket;
    }
    return TunnelEndpointKind::Tcp;
}

NSString *socksAuthToNS(SocksAuthMode mode)
{
    switch (mode) {
    case SocksAuthMode::UsernamePassword:
        return @"usernamePassword";
    case SocksAuthMode::None:
        return @"none";
    }
    return @"none";
}

SocksAuthMode socksAuthFromNS(NSString *value)
{
    if ([value isEqualToString:@"usernamePassword"] || [value isEqualToString:@"password"]) {
        return SocksAuthMode::UsernamePassword;
    }
    return SocksAuthMode::None;
}

NSDictionary *tunnelDefinitionToDict(const TunnelDefinition &def)
{
    return @{
        @"id" : uuidToNS(def.id) ?: [NSNull null],
        @"connectionId" : uuidToNS(def.connectionId) ?: [NSNull null],
        @"name" : qToNS(def.name),
        @"type" : tunnelTypeToNS(def.type),
        @"enabled" : @(def.enabled),
        @"localKind" : endpointKindToNS(def.localKind),
        @"localHost" : qToNS(def.localHost),
        @"localPort" : @(def.localPort),
        @"localSocketPath" : qToNS(def.localSocketPath),
        @"remoteKind" : endpointKindToNS(def.remoteKind),
        @"remoteHost" : qToNS(def.remoteHost),
        @"remotePort" : @(def.remotePort),
        @"remoteSocketPath" : qToNS(def.remoteSocketPath),
        @"socksAuth" : socksAuthToNS(def.socksAuth),
        @"socksUsername" : qToNS(def.socksUsername),
    };
}

TunnelDefinition tunnelDefinitionFromDict(NSDictionary *dict)
{
    TunnelDefinition def;
    if (dict == nil) {
        return def;
    }
    def.id = nsToUuid(dict[@"id"]);
    def.connectionId = nsToUuid(dict[@"connectionId"]);
    def.name = nsToQ(dict[@"name"] ?: @"");
    def.type = tunnelTypeFromNS(dict[@"type"] ?: @"local");
    def.enabled = [dict[@"enabled"] boolValue];
    def.localKind = endpointKindFromNS(dict[@"localKind"] ?: @"tcp");
    def.localHost = nsToQ(dict[@"localHost"] ?: @"127.0.0.1");
    def.localPort = static_cast<quint16>([dict[@"localPort"] integerValue]);
    def.localSocketPath = nsToQ(dict[@"localSocketPath"] ?: @"");
    def.remoteKind = endpointKindFromNS(dict[@"remoteKind"] ?: @"tcp");
    def.remoteHost = nsToQ(dict[@"remoteHost"] ?: @"127.0.0.1");
    def.remotePort = static_cast<quint16>([dict[@"remotePort"] integerValue]);
    def.remoteSocketPath = nsToQ(dict[@"remoteSocketPath"] ?: @"");
    def.socksAuth = socksAuthFromNS(dict[@"socksAuth"] ?: @"none");
    def.socksUsername = nsToQ(dict[@"socksUsername"] ?: @"");
    return def;
}

} // namespace

@implementation ESSTunnelStore

+ (NSArray<NSDictionary *> *)loadAll
{
    [EasySshRuntime start];
    const QList<TunnelDefinition> tunnels = TunnelStore::load();
    NSMutableArray<NSDictionary *> *out = [NSMutableArray array];
    for (const TunnelDefinition &def : tunnels) {
        [out addObject:tunnelDefinitionToDict(def)];
    }
    return out;
}

+ (NSArray<NSDictionary *> *)loadForConnectionId:(NSUUID *)connectionId
{
    [EasySshRuntime start];
    const QList<TunnelDefinition> tunnels = TunnelStore::loadForConnection(nsToUuid(connectionId));
    NSMutableArray<NSDictionary *> *out = [NSMutableArray array];
    for (const TunnelDefinition &def : tunnels) {
        [out addObject:tunnelDefinitionToDict(def)];
    }
    return out;
}

+ (void)saveAll:(NSArray<NSDictionary *> *)tunnels
{
    [EasySshRuntime start];
    QList<TunnelDefinition> list;
    for (NSDictionary *dict in tunnels) {
        list.append(tunnelDefinitionFromDict(dict));
    }
    TunnelStore::save(list);
}

+ (void)removeByConnectionId:(NSUUID *)connectionId
{
    [EasySshRuntime start];
    TunnelStore::removeByConnectionId(nsToUuid(connectionId));
}

@end
