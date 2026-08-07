/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSWorkspaceStore.h"
#import "EasySshRuntime.h"

#include "core/session/WorkspaceState.h"
#include "core/settings/AppSettings.h"

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

ESSWorkspaceState *toInfo(const WorkspaceState &state)
{
    ESSWorkspaceState *info = [[ESSWorkspaceState alloc] init];
    info.version = state.version;
    info.activeConnectionId = uuidToNS(state.activeConnectionId);
    NSMutableArray<ESSWorkspaceSessionEntry *> *sessions =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(state.sessions.size())];
    for (const WorkspaceSessionEntry &session : state.sessions) {
        ESSWorkspaceSessionEntry *entry = [[ESSWorkspaceSessionEntry alloc] init];
        entry.connectionId = uuidToNS(session.connectionId);
        entry.activeTerminalId = uuidToNS(session.activeTerminalId);
        entry.activeToolId = qToNS(session.activeToolId);
        NSMutableArray<ESSWorkspaceTerminalEntry *> *terminals =
            [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(session.terminals.size())];
        for (const WorkspaceTerminalEntry &shell : session.terminals) {
            ESSWorkspaceTerminalEntry *terminalInfo = [[ESSWorkspaceTerminalEntry alloc] init];
            terminalInfo.terminalId = uuidToNS(shell.id);
            terminalInfo.title = qToNS(shell.title);
            [terminals addObject:terminalInfo];
        }
        entry.terminals = terminals;
        NSMutableArray<NSString *> *tools =
            [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(session.tools.size())];
        for (const QString &tool : session.tools) {
            [tools addObject:qToNS(tool)];
        }
        entry.tools = tools;
        if (!session.dockState.isEmpty()) {
            entry.dockState = [NSData dataWithBytes:session.dockState.constData()
                                             length:static_cast<NSUInteger>(session.dockState.size())];
        }
        [sessions addObject:entry];
    }
    info.sessions = sessions;
    return info;
}

WorkspaceState fromInfo(ESSWorkspaceState *info)
{
    WorkspaceState state;
    if (info == nil) {
        return state;
    }
    state.version = static_cast<int>(info.version > 0 ? info.version : WorkspaceState::kCurrentVersion);
    state.activeConnectionId = nsToUuid(info.activeConnectionId);
    for (ESSWorkspaceSessionEntry *entry in info.sessions) {
        WorkspaceSessionEntry session;
        session.connectionId = nsToUuid(entry.connectionId);
        session.activeTerminalId = nsToUuid(entry.activeTerminalId);
        session.activeToolId = nsToQ(entry.activeToolId);
        for (ESSWorkspaceTerminalEntry *shell in entry.terminals) {
            WorkspaceTerminalEntry terminalEntry;
            terminalEntry.id = nsToUuid(shell.terminalId);
            terminalEntry.title = nsToQ(shell.title);
            session.terminals.append(terminalEntry);
        }
        for (NSString *tool in entry.tools) {
            session.tools.append(nsToQ(tool));
        }
        if (entry.dockState != nil && entry.dockState.length > 0) {
            session.dockState = QByteArray(static_cast<const char *>(entry.dockState.bytes),
                                           static_cast<int>(entry.dockState.length));
        }
        state.sessions.append(session);
    }
    return state;
}

} // namespace

@implementation ESSWorkspaceTerminalEntry
- (instancetype)init
{
    self = [super init];
    if (self) {
        _terminalId = [NSUUID UUID];
        _title = @"";
    }
    return self;
}
@end

@implementation ESSWorkspaceSessionEntry
- (instancetype)init
{
    self = [super init];
    if (self) {
        _connectionId = [NSUUID UUID];
        _activeToolId = @"";
        _terminals = @[];
        _tools = @[];
    }
    return self;
}
@end

@implementation ESSWorkspaceState
- (instancetype)init
{
    self = [super init];
    if (self) {
        _version = WorkspaceState::kCurrentVersion;
        _sessions = @[];
    }
    return self;
}

- (BOOL)isEmpty
{
    return self.sessions.count == 0;
}
@end

@implementation ESSWorkspaceStore

+ (ESSWorkspaceState *)loadState
{
    [EasySshRuntime start];
    bool ok = false;
    const WorkspaceState state =
        WorkspaceState::fromJson(AppSettings::instance().workspaceState(), &ok);
    if (!ok) {
        return [[ESSWorkspaceState alloc] init];
    }
    return toInfo(state);
}

+ (BOOL)save:(ESSWorkspaceState *)state error:(NSError *_Nullable *_Nullable)error
{
    [EasySshRuntime start];
    const WorkspaceState core = fromInfo(state);
    AppSettings::instance().setWorkspaceState(core.toJson());
    (void)error;
    return YES;
}

@end
