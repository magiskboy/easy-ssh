/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSAppSettings.h"
#import "EasySshRuntime.h"

#include "core/settings/AppSettings.h"

#include <QFont>
#include <QKeySequence>
#include <QObject>
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

void dispatchMain(dispatch_block_t block)
{
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_async(dispatch_get_main_queue(), block);
    }
}

} // namespace

@implementation ESSAppSettings {
    QMetaObject::Connection m_changedConnection;
}

+ (ESSAppSettings *)sharedSettings
{
    [EasySshRuntime start];
    static ESSAppSettings *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[ESSAppSettings alloc] init];
    });
    return shared;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [EasySshRuntime start];
        m_changedConnection = QObject::connect(
            &AppSettings::instance(), &AppSettings::settingsChanged, &AppSettings::instance(),
            [self]() {
                dispatchMain(^{
                    if (self.onSettingsChanged) {
                        self.onSettingsChanged();
                    }
                });
            });
    }
    return self;
}

- (void)dealloc
{
    QObject::disconnect(m_changedConnection);
}

#define ESS_BOOL_PROP(getter, setter, coreGetter, coreSetter)                                      \
    - (BOOL)getter                                                                                 \
    {                                                                                              \
        return AppSettings::instance().coreGetter();                                               \
    }                                                                                              \
    - (void)setter : (BOOL)value                                                                   \
    {                                                                                              \
        AppSettings::instance().coreSetter(value);                                                 \
    }

ESS_BOOL_PROP(showSizeColumn, setShowSizeColumn, showSizeColumn, setShowSizeColumn)
ESS_BOOL_PROP(showPermissionsColumn, setShowPermissionsColumn, showPermissionsColumn,
              setShowPermissionsColumn)
ESS_BOOL_PROP(showModifiedColumn, setShowModifiedColumn, showModifiedColumn, setShowModifiedColumn)
ESS_BOOL_PROP(showHiddenFiles, setShowHiddenFiles, showHiddenFiles, setShowHiddenFiles)
ESS_BOOL_PROP(cursorBlink, setCursorBlink, cursorBlink, setCursorBlink)
ESS_BOOL_PROP(confirmMultilinePaste, setConfirmMultilinePaste, confirmMultilinePaste,
              setConfirmMultilinePaste)
ESS_BOOL_PROP(smartLayout, setSmartLayout, smartLayout, setSmartLayout)
ESS_BOOL_PROP(autoReconnect, setAutoReconnect, autoReconnect, setAutoReconnect)
ESS_BOOL_PROP(restoreWorkspace, setRestoreWorkspace, restoreWorkspace, setRestoreWorkspace)
ESS_BOOL_PROP(closeToTray, setCloseToTray, closeToTray, setCloseToTray)
ESS_BOOL_PROP(minimizeToTray, setMinimizeToTray, minimizeToTray, setMinimizeToTray)
ESS_BOOL_PROP(startInTray, setStartInTray, startInTray, setStartInTray)
ESS_BOOL_PROP(trayNotifications, setTrayNotifications, trayNotifications, setTrayNotifications)
ESS_BOOL_PROP(trayMinimizeHintShown, setTrayMinimizeHintShown, trayMinimizeHintShown,
              setTrayMinimizeHintShown)
ESS_BOOL_PROP(autoResumeTransferAfterReconnect, setAutoResumeTransferAfterReconnect,
              autoResumeTransferAfterReconnect, setAutoResumeTransferAfterReconnect)

#undef ESS_BOOL_PROP

- (NSString *)defaultDownloadDir
{
    return qToNS(AppSettings::instance().defaultDownloadDir());
}

- (void)setDefaultDownloadDir:(NSString *)defaultDownloadDir
{
    AppSettings::instance().setDefaultDownloadDir(nsToQ(defaultDownloadDir));
}

- (NSString *)terminalFontFamily
{
    return qToNS(AppSettings::instance().terminalFont().family());
}

- (void)setTerminalFontFamily:(NSString *)terminalFontFamily
{
    QFont font = AppSettings::instance().terminalFont();
    font.setFamily(nsToQ(terminalFontFamily));
    AppSettings::instance().setTerminalFont(font);
}

- (double)terminalFontPointSize
{
    return AppSettings::instance().terminalFont().pointSizeF();
}

- (void)setTerminalFontPointSize:(double)terminalFontPointSize
{
    QFont font = AppSettings::instance().terminalFont();
    font.setPointSizeF(terminalFontPointSize > 0 ? terminalFontPointSize : 10.0);
    AppSettings::instance().setTerminalFont(font);
}

- (NSString *)colorScheme
{
    return qToNS(AppSettings::instance().colorScheme());
}

- (void)setColorScheme:(NSString *)colorScheme
{
    AppSettings::instance().setColorScheme(nsToQ(colorScheme));
}

- (NSInteger)historySize
{
    return AppSettings::instance().historySize();
}

- (void)setHistorySize:(NSInteger)historySize
{
    AppSettings::instance().setHistorySize(static_cast<int>(historySize));
}

- (NSInteger)cursorShape
{
    return AppSettings::instance().cursorShape();
}

- (void)setCursorShape:(NSInteger)cursorShape
{
    AppSettings::instance().setCursorShape(static_cast<int>(cursorShape));
}

- (NSInteger)transferStallTimeoutSec
{
    return AppSettings::instance().transferStallTimeoutSec();
}

- (void)setTransferStallTimeoutSec:(NSInteger)transferStallTimeoutSec
{
    AppSettings::instance().setTransferStallTimeoutSec(static_cast<int>(transferStallTimeoutSec));
}

- (NSString *)uiFontMode
{
    return qToNS(AppSettings::instance().uiFontMode());
}

- (void)setUiFontMode:(NSString *)uiFontMode
{
    AppSettings::instance().setUiFontMode(nsToQ(uiFontMode));
}

- (NSString *)uiFontFamily
{
    return qToNS(AppSettings::instance().uiFont().family());
}

- (void)setUiFontFamily:(NSString *)uiFontFamily
{
    QFont font = AppSettings::instance().uiFont();
    font.setFamily(nsToQ(uiFontFamily));
    AppSettings::instance().setUiFont(font);
}

- (double)uiFontPointSize
{
    return AppSettings::instance().uiFont().pointSizeF();
}

- (void)setUiFontPointSize:(double)uiFontPointSize
{
    QFont font = AppSettings::instance().uiFont();
    font.setPointSizeF(uiFontPointSize > 0 ? uiFontPointSize : 13.0);
    AppSettings::instance().setUiFont(font);
}

- (NSString *)themeId
{
    return qToNS(AppSettings::instance().themeId());
}

- (void)setThemeId:(NSString *)themeId
{
    AppSettings::instance().setThemeId(nsToQ(themeId));
}

- (NSString *)customThemePath
{
    return qToNS(AppSettings::instance().customThemePath());
}

- (void)setCustomThemePath:(NSString *)customThemePath
{
    AppSettings::instance().setCustomThemePath(nsToQ(customThemePath));
}

- (NSData *)windowGeometry
{
    const QByteArray bytes = AppSettings::instance().windowGeometry();
    if (bytes.isEmpty()) {
        return nil;
    }
    return [NSData dataWithBytes:bytes.constData() length:static_cast<NSUInteger>(bytes.size())];
}

- (void)setWindowGeometry:(NSData *)windowGeometry
{
    if (windowGeometry == nil || windowGeometry.length == 0) {
        AppSettings::instance().setWindowGeometry({});
        return;
    }
    AppSettings::instance().setWindowGeometry(
        QByteArray(static_cast<const char *>(windowGeometry.bytes),
                   static_cast<int>(windowGeometry.length)));
}

- (NSInteger)sidebarWidth
{
    return AppSettings::instance().sidebarWidth();
}

- (void)setSidebarWidth:(NSInteger)sidebarWidth
{
    AppSettings::instance().setSidebarWidth(static_cast<int>(sidebarWidth));
}

- (NSString *)sidebarTabId
{
    return qToNS(AppSettings::instance().sidebarTabId());
}

- (void)setSidebarTabId:(NSString *)sidebarTabId
{
    AppSettings::instance().setSidebarTabId(nsToQ(sidebarTabId));
}

- (NSData *)workspaceStateData
{
    const QByteArray bytes = AppSettings::instance().workspaceState();
    if (bytes.isEmpty()) {
        return nil;
    }
    return [NSData dataWithBytes:bytes.constData() length:static_cast<NSUInteger>(bytes.size())];
}

- (void)setWorkspaceStateData:(NSData *)workspaceStateData
{
    if (workspaceStateData == nil || workspaceStateData.length == 0) {
        AppSettings::instance().setWorkspaceState({});
        return;
    }
    AppSettings::instance().setWorkspaceState(
        QByteArray(static_cast<const char *>(workspaceStateData.bytes),
                   static_cast<int>(workspaceStateData.length)));
}

- (NSArray<NSUUID *> *)recentConnectionIdsWithLimit:(NSInteger)limit
{
    const QList<QUuid> ids =
        AppSettings::instance().recentConnectionIds(static_cast<int>(limit > 0 ? limit : 8));
    NSMutableArray<NSUUID *> *out =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(ids.size())];
    for (const QUuid &id : ids) {
        NSUUID *nsId =
            [[NSUUID alloc] initWithUUIDString:qToNS(id.toString(QUuid::WithoutBraces))];
        if (nsId != nil) {
            [out addObject:nsId];
        }
    }
    return out;
}

- (void)recordRecentConnection:(NSUUID *)connectionId
{
    if (connectionId == nil) {
        return;
    }
    AppSettings::instance().recordRecentConnection(QUuid(nsToQ(connectionId.UUIDString)));
}

- (NSString *)shortcutForActionId:(NSString *)actionId
{
    const QKeySequence seq = AppSettings::instance().shortcut(nsToQ(actionId));
    if (seq.isEmpty()) {
        return nil;
    }
    return qToNS(seq.toString(QKeySequence::PortableText));
}

- (void)setShortcut:(NSString *)sequence forActionId:(NSString *)actionId
{
    AppSettings::instance().setShortcut(nsToQ(actionId),
                                        QKeySequence(nsToQ(sequence), QKeySequence::PortableText));
}

- (void)resetShortcutsToDefaults
{
    AppSettings::instance().resetShortcutsToDefaults();
}

+ (NSArray<NSString *> *)shortcutActionIds
{
    [EasySshRuntime start];
    const QStringList ids = AppSettings::shortcutActionIds();
    NSMutableArray<NSString *> *out =
        [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(ids.size())];
    for (const QString &id : ids) {
        [out addObject:qToNS(id)];
    }
    return out;
}

+ (NSString *)defaultShortcutForActionId:(NSString *)actionId
{
    [EasySshRuntime start];
    return qToNS(AppSettings::defaultShortcut(nsToQ(actionId)).toString(QKeySequence::PortableText));
}

+ (NSString *)shortcutLabelForActionId:(NSString *)actionId
{
    [EasySshRuntime start];
    return qToNS(AppSettings::shortcutLabel(nsToQ(actionId)));
}

+ (NSString *)shortcutGroupForActionId:(NSString *)actionId
{
    [EasySshRuntime start];
    return qToNS(AppSettings::shortcutGroup(nsToQ(actionId)));
}

- (void)notifyChanged
{
    AppSettings::instance().notifyChanged();
}

@end
