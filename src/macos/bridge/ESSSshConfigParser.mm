/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSSshConfigParser.h"
#import "ESSConnectionMapping.h"
#import "EasySshRuntime.h"

#include "core/connection/SshConfigParser.h"

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

ESSSshConfigHost *toHostInfo(const SshConfigHost &host)
{
    ESSSshConfigHost *info = [[ESSSshConfigHost alloc] init];
    info.aliasName = qToNS(host.alias);
    info.hostName = qToNS(host.hostName);
    info.user = qToNS(host.user);
    info.port = host.port;
    NSMutableArray<NSString *> *files =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(host.identityFiles.size())];
    for (const QString &path : host.identityFiles) {
        [files addObject:qToNS(path)];
    }
    info.identityFiles = files;
    info.proxyJump = qToNS(host.proxyJump);
    info.proxyCommand = qToNS(host.proxyCommand);
    info.forwardAgent = host.forwardAgent;
    return info;
}

} // namespace

@implementation ESSSshConfigHost
- (instancetype)init
{
    self = [super init];
    if (self) {
        _aliasName = @"";
        _hostName = @"";
        _user = @"";
        _port = 22;
        _identityFiles = @[];
        _proxyJump = @"";
        _proxyCommand = @"";
        _forwardAgent = NO;
    }
    return self;
}
@end

@implementation ESSSshConfigParser

+ (NSString *)defaultConfigPath
{
    [EasySshRuntime start];
    return qToNS(SshConfigParser::defaultConfigPath());
}

+ (NSArray<ESSSshConfigHost *> *)loadHostsFromPath:(NSString *)path
{
    [EasySshRuntime start];
    const QList<SshConfigHost> hosts = SshConfigParser::load(nsToQ(path));
    NSMutableArray<ESSSshConfigHost *> *out =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(hosts.size())];
    for (const SshConfigHost &host : hosts) {
        [out addObject:toHostInfo(host)];
    }
    return out;
}

+ (NSArray<ESSConnectionInfo *> *)connectionsFromConfigPath:(NSString *)path
{
    [EasySshRuntime start];
    const QString configPath = path != nil ? nsToQ(path) : QString();
    const QList<SshConfigHost> hosts = SshConfigParser::load(configPath);
    const QList<Connection> connections = SshConfigParser::toConnections(hosts, configPath);
    NSMutableArray<ESSConnectionInfo *> *out =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(connections.size())];
    for (const Connection &c : connections) {
        [out addObject:essConnectionToInfo(c)];
    }
    return out;
}

@end
