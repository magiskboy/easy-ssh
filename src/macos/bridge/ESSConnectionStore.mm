/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSConnectionStore.h"
#import "ESSConnectionMapping.h"
#import "EasySshRuntime.h"

#include "core/connection/ConnectionStore.h"

#include <QUuid>

@implementation ESSJumpHop
- (instancetype)init
{
    self = [super init];
    if (self) {
        _host = @"";
        _port = 22;
        _username = @"";
        _authType = ESSAuthTypePassword;
        _useTargetCredentials = YES;
    }
    return self;
}
@end

@implementation ESSShellCommandSet
- (instancetype)init
{
    self = [super init];
    if (self) {
        _shell = @"";
        _listingCommand = @"";
        _listFileCommand = @"";
        _mkdirCommand = @"";
        _removeCommand = @"";
        _renameCommand = @"";
        _pwdCommand = @"";
        _realpathCommand = @"";
        _symlinkCommand = @"";
        _readlinkCommand = @"";
        _clearAliases = YES;
        _clearNationalVars = YES;
        _tryFullTime = YES;
        _ignoreLsWarnings = NO;
        _allowScpFallback = YES;
    }
    return self;
}
@end

@implementation ESSConnectionInfo
- (instancetype)init
{
    self = [super init];
    if (self) {
        _connectionId = [NSUUID UUID];
        _name = @"";
        _host = @"";
        _port = 22;
        _username = @"";
        _authType = ESSAuthTypePassword;
        _savePassword = NO;
        _startupDirectory = @"";
        _source = ESSConnectionSourceApp;
        _configAlias = @"";
        _proxyMode = ESSProxyModeNone;
        _jumpHops = @[];
        _proxyCommand = @"";
        _keepAliveIntervalSec = 0;
        _keepAliveCountMax = 3;
        _compressionEnabled = NO;
        _agentForwarding = NO;
        _shellCommands = [[ESSShellCommandSet alloc] init];
        _displayText = @"";
    }
    return self;
}
@end

@implementation ESSSessionCredentials
@end

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

ESSJumpHop *toHopInfo(const JumpHop &hop)
{
    ESSJumpHop *info = [[ESSJumpHop alloc] init];
    info.host = qToNS(hop.host);
    info.port = hop.port;
    info.username = qToNS(hop.username);
    info.authType =
        hop.authType == AuthType::PrivateKey ? ESSAuthTypePrivateKey : ESSAuthTypePassword;
    info.privateKeyPath = hop.privateKeyPath.isEmpty() ? nil : qToNS(hop.privateKeyPath);
    info.useTargetCredentials = hop.useTargetCredentials;
    return info;
}

JumpHop fromHopInfo(ESSJumpHop *info)
{
    JumpHop hop;
    if (info == nil) {
        return hop;
    }
    hop.host = nsToQ(info.host);
    hop.port = static_cast<quint16>(info.port > 0 ? info.port : 22);
    hop.username = nsToQ(info.username);
    hop.authType = info.authType == ESSAuthTypePrivateKey ? AuthType::PrivateKey : AuthType::Password;
    if (info.privateKeyPath != nil) {
        hop.privateKeyPath = nsToQ(info.privateKeyPath);
    }
    hop.useTargetCredentials = info.useTargetCredentials;
    return hop;
}

ESSShellCommandSet *toShellInfo(const ShellCommandSetConfig &cfg)
{
    ESSShellCommandSet *info = [[ESSShellCommandSet alloc] init];
    info.shell = qToNS(cfg.shell);
    info.listingCommand = qToNS(cfg.listingCommand);
    info.listFileCommand = qToNS(cfg.listFileCommand);
    info.mkdirCommand = qToNS(cfg.mkdirCommand);
    info.removeCommand = qToNS(cfg.removeCommand);
    info.renameCommand = qToNS(cfg.renameCommand);
    info.pwdCommand = qToNS(cfg.pwdCommand);
    info.realpathCommand = qToNS(cfg.realpathCommand);
    info.symlinkCommand = qToNS(cfg.symlinkCommand);
    info.readlinkCommand = qToNS(cfg.readlinkCommand);
    info.clearAliases = cfg.clearAliases;
    info.clearNationalVars = cfg.clearNationalVars;
    info.tryFullTime = cfg.tryFullTime;
    info.ignoreLsWarnings = cfg.ignoreLsWarnings;
    info.allowScpFallback = cfg.allowScpFallback;
    return info;
}

ShellCommandSetConfig fromShellInfo(ESSShellCommandSet *info)
{
    ShellCommandSetConfig cfg;
    if (info == nil) {
        return cfg;
    }
    cfg.shell = nsToQ(info.shell);
    cfg.listingCommand = nsToQ(info.listingCommand);
    cfg.listFileCommand = nsToQ(info.listFileCommand);
    cfg.mkdirCommand = nsToQ(info.mkdirCommand);
    cfg.removeCommand = nsToQ(info.removeCommand);
    cfg.renameCommand = nsToQ(info.renameCommand);
    cfg.pwdCommand = nsToQ(info.pwdCommand);
    cfg.realpathCommand = nsToQ(info.realpathCommand);
    cfg.symlinkCommand = nsToQ(info.symlinkCommand);
    cfg.readlinkCommand = nsToQ(info.readlinkCommand);
    cfg.clearAliases = info.clearAliases;
    cfg.clearNationalVars = info.clearNationalVars;
    cfg.tryFullTime = info.tryFullTime;
    cfg.ignoreLsWarnings = info.ignoreLsWarnings;
    cfg.allowScpFallback = info.allowScpFallback;
    return cfg;
}

} // namespace

Connection essConnectionFromInfo(ESSConnectionInfo *info)
{
    Connection c;
    if (info == nil) {
        return c;
    }
    if (info.connectionId != nil) {
        c.id = QUuid(nsToQ(info.connectionId.UUIDString));
    }
    if (c.id.isNull()) {
        c.id = QUuid::createUuid();
    }
    c.name = nsToQ(info.name);
    c.host = nsToQ(info.host);
    c.port = static_cast<quint16>(info.port > 0 ? info.port : 22);
    c.username = nsToQ(info.username);
    c.authType = info.authType == ESSAuthTypePrivateKey ? AuthType::PrivateKey : AuthType::Password;
    c.savePassword = info.savePassword;
    if (info.privateKeyPath != nil) {
        c.privateKeyPath = nsToQ(info.privateKeyPath);
    }
    c.startupDirectory = nsToQ(info.startupDirectory);
    c.source = info.source == ESSConnectionSourceSshConfig ? ConnectionSource::SshConfig
                                                           : ConnectionSource::App;
    c.configAlias = nsToQ(info.configAlias);
    c.proxyMode = static_cast<SshProxyMode>(info.proxyMode);
    c.proxyCommand = nsToQ(info.proxyCommand);
    c.keepAliveIntervalSec = static_cast<int>(info.keepAliveIntervalSec);
    c.keepAliveCountMax = static_cast<int>(info.keepAliveCountMax);
    c.compressionEnabled = info.compressionEnabled;
    c.agentForwarding = info.agentForwarding;
    c.shellCommands = fromShellInfo(info.shellCommands);
    c.jumpHops.clear();
    for (ESSJumpHop *hop in info.jumpHops) {
        c.jumpHops.append(fromHopInfo(hop));
    }
    c.normalizeProxyFields();
    return c;
}

ESSConnectionInfo *essConnectionToInfo(const Connection &c)
{
    ESSConnectionInfo *info = [[ESSConnectionInfo alloc] init];
    info.connectionId =
        [[NSUUID alloc] initWithUUIDString:qToNS(c.id.toString(QUuid::WithoutBraces))];
    info.name = qToNS(c.name);
    info.host = qToNS(c.host);
    info.port = c.port;
    info.username = qToNS(c.username);
    info.authType =
        c.authType == AuthType::PrivateKey ? ESSAuthTypePrivateKey : ESSAuthTypePassword;
    info.savePassword = c.savePassword;
    info.privateKeyPath = c.privateKeyPath.isEmpty() ? nil : qToNS(c.privateKeyPath);
    info.startupDirectory = qToNS(c.startupDirectory);
    info.source = c.source == ConnectionSource::SshConfig ? ESSConnectionSourceSshConfig
                                                          : ESSConnectionSourceApp;
    info.configAlias = qToNS(c.configAlias);
    info.proxyMode = static_cast<ESSProxyMode>(c.proxyMode);
    info.proxyCommand = qToNS(c.proxyCommand);
    info.keepAliveIntervalSec = c.keepAliveIntervalSec;
    info.keepAliveCountMax = c.keepAliveCountMax;
    info.compressionEnabled = c.compressionEnabled;
    info.agentForwarding = c.agentForwarding;
    info.shellCommands = toShellInfo(c.shellCommands);
    NSMutableArray<ESSJumpHop *> *hops =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(c.jumpHops.size())];
    for (const JumpHop &hop : c.jumpHops) {
        [hops addObject:toHopInfo(hop)];
    }
    info.jumpHops = hops;
    info.displayText = qToNS(c.displayText());
    return info;
}

SessionCredentials essCredentialsFromInfo(ESSSessionCredentials *creds)
{
    SessionCredentials out;
    if (creds == nil) {
        return out;
    }
    if (creds.targetSecret != nil) {
        out.targetSecret = nsToQ(creds.targetSecret);
    }
    if (creds.gatewaySecret != nil) {
        out.gatewaySecret = nsToQ(creds.gatewaySecret);
    }
    return out;
}

@implementation ESSConnectionStore

+ (NSArray<ESSConnectionInfo *> *)loadConnections
{
    [EasySshRuntime start];
    const QList<Connection> list = ConnectionStore::load();
    NSMutableArray<ESSConnectionInfo *> *out =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(list.size())];
    for (const Connection &c : list) {
        [out addObject:essConnectionToInfo(c)];
    }
    return out;
}

+ (BOOL)saveConnections:(NSArray<ESSConnectionInfo *> *)connections
                  error:(NSError *_Nullable *_Nullable)error
{
    [EasySshRuntime start];
    QList<Connection> list;
    list.reserve(static_cast<int>(connections.count));
    for (ESSConnectionInfo *info in connections) {
        list.append(essConnectionFromInfo(info));
    }
    ConnectionStore::save(list);
    (void)error;
    return YES;
}

@end
