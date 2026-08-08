// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "AppSettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QUuid>
#include <QtGlobal>

#include <iterator>

namespace
{

constexpr auto kShowSize = "fileExplorer/showSize";
constexpr auto kShowPermissions = "fileExplorer/showPermissions";
constexpr auto kShowModified = "fileExplorer/showModified";
constexpr auto kShowHidden = "fileExplorer/showHiddenFiles";
constexpr auto kDownloadDir = "fileExplorer/defaultDownloadDir";

constexpr auto kTerminalFont = "terminal/font";
constexpr auto kColorScheme = "terminal/colorScheme";
constexpr auto kHistorySize = "terminal/historySize";
constexpr auto kCursorShape = "terminal/cursorShape";
constexpr auto kCursorBlink = "terminal/cursorBlink";
constexpr auto kConfirmMultilinePaste = "terminal/confirmMultilinePaste";
constexpr auto kSmartLayout = "terminal/smartLayout";

constexpr auto kAutoReconnect = "session/autoReconnect";
constexpr auto kRestoreWorkspace = "session/restoreWorkspace";
constexpr auto kWorkspaceState = "session/workspaceState";
constexpr auto kTransferStallTimeoutSec = "transfer/stallTimeoutSec";
constexpr auto kAutoResumeTransfer = "transfer/autoResumeAfterReconnect";
constexpr auto kRecentConnections = "session/recentConnectionIds";
constexpr int kMaxRecentConnections = 8;
constexpr int kDefaultStallTimeoutSec = 60;

constexpr auto kUiFontMode = "ui/fontMode";
constexpr auto kUiFont = "ui/font";
constexpr auto kThemeId = "ui/themeId";
constexpr auto kCustomThemePath = "ui/customThemePath";
constexpr auto kWindowGeometry = "ui/window/geometry";
constexpr auto kCloseToTray = "ui/window/closeToTray";
constexpr auto kMinimizeToTray = "ui/window/minimizeToTray";
constexpr auto kStartInTray = "ui/window/startInTray";
constexpr auto kTrayNotifications = "ui/window/trayNotifications";
constexpr auto kTrayMinimizeHintShown = "ui/window/trayMinimizeHintShown";
constexpr auto kSidebarWidth = "ui/sidebar/width";
constexpr auto kSidebarTabId = "ui/sidebar/tabId";
constexpr auto kSidebarTabIndexLegacy = "ui/sidebar/tabIndex";

struct ShortcutDef
{
    const char *id;
    const char *group;
    const char *label;
    const char *sequence;
};

constexpr ShortcutDef kShortcutDefs[] = {
    {"general.newConnection", "General", "New Connection", "Ctrl+N"},
    {"general.connectionManager", "General", "Connection Manager", "Ctrl+Shift+O"},
    {"general.quickConnect", "General", "Quick Connect", "Ctrl+K"},
    {"general.commandPalette", "General", "Command Palette", "Ctrl+Shift+P"},
    {"general.settings", "General", "Settings", "Ctrl+,"},
    {"general.shortcuts", "General", "Keyboard Shortcuts", "Ctrl+Shift+,"},
    {"general.about", "General", "About", "F1"},

    {"session.newSession", "Terminal", "New Terminal", "Ctrl+T"},
    {"session.goToTerminal", "Windows", "Go to Terminal", "Ctrl+P"},
    {"session.processExplorer", "Explorer", "Processes", ""},
    {"session.containerExplorer", "Explorer", "Containers", ""},
    {"session.serviceExplorer", "Explorer", "Services", ""},
    {"session.systemInfo", "Explorer", "System Information", ""},
    {"session.nextTab", "Windows", "Next Tab", "Ctrl+Tab"},
    {"session.previousTab", "Windows", "Previous Tab", "Ctrl+Shift+Tab"},
    {"terminal.close", "Terminal", "Close Terminal", "Ctrl+Shift+W"},

    {"terminal.copy", "Terminal", "Copy", "Ctrl+Shift+C"},
    {"terminal.paste", "Terminal", "Paste", "Ctrl+Shift+V"},
    {"terminal.clearScreen", "Terminal", "Clear Screen", "Ctrl+L"},
    {"terminal.search", "Terminal", "Search", "Ctrl+Shift+F"},
    {"terminal.saveLog", "Terminal", "Save Log", "Ctrl+S"},
    {"terminal.saveScreenshot", "Terminal", "Save Screenshot", "Ctrl+Shift+S"},

    {"fileExplorer.rename", "File Explorer", "Rename", "F2"},
    {"fileExplorer.delete", "File Explorer", "Delete", "Delete"},
    {"fileExplorer.refresh", "File Explorer", "Refresh", "F5"},
    {"fileExplorer.upload", "File Explorer", "Upload", "Ctrl+U"},
    {"fileExplorer.download", "File Explorer", "Download", "Ctrl+D"},
    {"fileExplorer.openWith", "File Explorer", "Open With", "Ctrl+O"},
};

const ShortcutDef *findShortcutDef(const QString &actionId)
{
    for (const ShortcutDef &def : kShortcutDefs) {
        if (actionId == QLatin1String(def.id)) {
            return &def;
        }
    }
    return nullptr;
}

} // namespace

AppSettings &AppSettings::instance()
{
    static AppSettings settings;
    return settings;
}

AppSettings::AppSettings(QObject *parent) : QObject(parent) {}

bool AppSettings::boolValue(const QString &key, bool defaultValue) const
{
    QSettings settings;
    return settings.value(key, defaultValue).toBool();
}

void AppSettings::setBoolValue(const QString &key, bool value)
{
    QSettings settings;
    settings.setValue(key, value);
}

int AppSettings::intValue(const QString &key, int defaultValue) const
{
    QSettings settings;
    return settings.value(key, defaultValue).toInt();
}

void AppSettings::setIntValue(const QString &key, int value)
{
    QSettings settings;
    settings.setValue(key, value);
}

QString AppSettings::stringValue(const QString &key, const QString &defaultValue) const
{
    QSettings settings;
    return settings.value(key, defaultValue).toString();
}

void AppSettings::setStringValue(const QString &key, const QString &value)
{
    QSettings settings;
    settings.setValue(key, value);
}

bool AppSettings::showSizeColumn() const
{
    return boolValue(QLatin1String(kShowSize), true);
}

void AppSettings::setShowSizeColumn(bool value)
{
    setBoolValue(QLatin1String(kShowSize), value);
}

bool AppSettings::showPermissionsColumn() const
{
    return boolValue(QLatin1String(kShowPermissions), true);
}

void AppSettings::setShowPermissionsColumn(bool value)
{
    setBoolValue(QLatin1String(kShowPermissions), value);
}

bool AppSettings::showModifiedColumn() const
{
    return boolValue(QLatin1String(kShowModified), false);
}

void AppSettings::setShowModifiedColumn(bool value)
{
    setBoolValue(QLatin1String(kShowModified), value);
}

bool AppSettings::showHiddenFiles() const
{
    return boolValue(QLatin1String(kShowHidden), false);
}

void AppSettings::setShowHiddenFiles(bool value)
{
    setBoolValue(QLatin1String(kShowHidden), value);
}

QString AppSettings::defaultDownloadDir() const
{
    return stringValue(QLatin1String(kDownloadDir), QString());
}

void AppSettings::setDefaultDownloadDir(const QString &path)
{
    setStringValue(QLatin1String(kDownloadDir), path);
}

QList<QUuid> AppSettings::recentConnectionIds(int limit) const
{
    QSettings settings;
    const QStringList stored = settings.value(QLatin1String(kRecentConnections)).toStringList();
    QList<QUuid> ids;
    ids.reserve(stored.size());
    for (const QString &value : stored) {
        const QUuid id = QUuid::fromString(value);
        if (!id.isNull()) {
            ids.append(id);
        }
        if (limit > 0 && ids.size() >= limit) {
            break;
        }
    }
    return ids;
}

void AppSettings::recordRecentConnection(const QUuid &id)
{
    if (id.isNull()) {
        return;
    }

    QList<QUuid> ids = recentConnectionIds(0);
    ids.removeAll(id);
    ids.prepend(id);
    while (ids.size() > kMaxRecentConnections) {
        ids.removeLast();
    }

    QStringList stored;
    stored.reserve(ids.size());
    for (const QUuid &entry : ids) {
        stored.append(entry.toString(QUuid::WithoutBraces));
    }

    QSettings settings;
    settings.setValue(QLatin1String(kRecentConnections), stored);
}

QFont AppSettings::terminalFont() const
{
    QFont font(QStringLiteral("Monospace"), 10);
    font.setStyleHint(QFont::TypeWriter);
    const QString stored = stringValue(QLatin1String(kTerminalFont), QString());
    if (!stored.isEmpty()) {
        font.fromString(stored);
    }
    return font;
}

void AppSettings::setTerminalFont(const QFont &font)
{
    setStringValue(QLatin1String(kTerminalFont), font.toString());
}

QString AppSettings::colorScheme() const
{
    return stringValue(QLatin1String(kColorScheme), QStringLiteral("WhiteOnBlack"));
}

void AppSettings::setColorScheme(const QString &scheme)
{
    setStringValue(QLatin1String(kColorScheme), scheme);
}

int AppSettings::historySize() const
{
    return intValue(QLatin1String(kHistorySize), 10000);
}

void AppSettings::setHistorySize(int lines)
{
    setIntValue(QLatin1String(kHistorySize), lines);
}

int AppSettings::cursorShape() const
{
    return intValue(QLatin1String(kCursorShape), 0);
}

void AppSettings::setCursorShape(int shape)
{
    setIntValue(QLatin1String(kCursorShape), qBound(0, shape, 2));
}

bool AppSettings::cursorBlink() const
{
    return boolValue(QLatin1String(kCursorBlink), true);
}

void AppSettings::setCursorBlink(bool blink)
{
    setBoolValue(QLatin1String(kCursorBlink), blink);
}

bool AppSettings::confirmMultilinePaste() const
{
    return boolValue(QLatin1String(kConfirmMultilinePaste), true);
}

void AppSettings::setConfirmMultilinePaste(bool confirm)
{
    setBoolValue(QLatin1String(kConfirmMultilinePaste), confirm);
}

bool AppSettings::smartLayout() const
{
    return boolValue(QLatin1String(kSmartLayout), true);
}

void AppSettings::setSmartLayout(bool enabled)
{
    setBoolValue(QLatin1String(kSmartLayout), enabled);
}

bool AppSettings::autoReconnect() const
{
    return boolValue(QLatin1String(kAutoReconnect), false);
}

void AppSettings::setAutoReconnect(bool enabled)
{
    setBoolValue(QLatin1String(kAutoReconnect), enabled);
}

bool AppSettings::restoreWorkspace() const
{
    return boolValue(QLatin1String(kRestoreWorkspace), true);
}

void AppSettings::setRestoreWorkspace(bool enabled)
{
    setBoolValue(QLatin1String(kRestoreWorkspace), enabled);
}

bool AppSettings::closeToTray() const
{
    return boolValue(QLatin1String(kCloseToTray), true);
}

void AppSettings::setCloseToTray(bool enabled)
{
    setBoolValue(QLatin1String(kCloseToTray), enabled);
}

bool AppSettings::minimizeToTray() const
{
    return boolValue(QLatin1String(kMinimizeToTray), false);
}

void AppSettings::setMinimizeToTray(bool enabled)
{
    setBoolValue(QLatin1String(kMinimizeToTray), enabled);
}

bool AppSettings::startInTray() const
{
    return boolValue(QLatin1String(kStartInTray), false);
}

void AppSettings::setStartInTray(bool enabled)
{
    setBoolValue(QLatin1String(kStartInTray), enabled);
}

bool AppSettings::trayNotifications() const
{
    return boolValue(QLatin1String(kTrayNotifications), true);
}

void AppSettings::setTrayNotifications(bool enabled)
{
    setBoolValue(QLatin1String(kTrayNotifications), enabled);
}

bool AppSettings::trayMinimizeHintShown() const
{
    return boolValue(QLatin1String(kTrayMinimizeHintShown), false);
}

void AppSettings::setTrayMinimizeHintShown(bool shown)
{
    setBoolValue(QLatin1String(kTrayMinimizeHintShown), shown);
}

QByteArray AppSettings::workspaceState() const
{
    QSettings settings;
    return settings.value(QLatin1String(kWorkspaceState)).toByteArray();
}

void AppSettings::setWorkspaceState(const QByteArray &state)
{
    QSettings settings;
    settings.setValue(QLatin1String(kWorkspaceState), state);
}

int AppSettings::transferStallTimeoutSec() const
{
    return qMax(0, intValue(QLatin1String(kTransferStallTimeoutSec), kDefaultStallTimeoutSec));
}

void AppSettings::setTransferStallTimeoutSec(int seconds)
{
    setIntValue(QLatin1String(kTransferStallTimeoutSec), qMax(0, seconds));
}

bool AppSettings::autoResumeTransferAfterReconnect() const
{
    return boolValue(QLatin1String(kAutoResumeTransfer), true);
}

void AppSettings::setAutoResumeTransferAfterReconnect(bool enabled)
{
    setBoolValue(QLatin1String(kAutoResumeTransfer), enabled);
}

QString AppSettings::uiFontMode() const
{
    return stringValue(QLatin1String(kUiFontMode), QStringLiteral("system"));
}

void AppSettings::setUiFontMode(const QString &mode)
{
    setStringValue(QLatin1String(kUiFontMode), mode);
}

QFont AppSettings::uiFont() const
{
    QFont font;
    const QString stored = stringValue(QLatin1String(kUiFont), QString());
    if (!stored.isEmpty()) {
        font.fromString(stored);
    }
    return font;
}

void AppSettings::setUiFont(const QFont &font)
{
    setStringValue(QLatin1String(kUiFont), font.toString());
}

QString AppSettings::themeId() const
{
    return stringValue(QLatin1String(kThemeId), QStringLiteral("system"));
}

void AppSettings::setThemeId(const QString &id)
{
    setStringValue(QLatin1String(kThemeId), id);
}

QString AppSettings::customThemePath() const
{
    return stringValue(QLatin1String(kCustomThemePath), QString());
}

void AppSettings::setCustomThemePath(const QString &path)
{
    setStringValue(QLatin1String(kCustomThemePath), path);
}

QByteArray AppSettings::windowGeometry() const
{
    QSettings settings;
    return settings.value(QLatin1String(kWindowGeometry)).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray &geometry)
{
    QSettings settings;
    settings.setValue(QLatin1String(kWindowGeometry), geometry);
}

int AppSettings::sidebarWidth() const
{
    const int width = intValue(QLatin1String(kSidebarWidth), kSidebarDefaultWidth);
    return qBound(kSidebarMinWidth, width, kSidebarMaxWidth);
}

void AppSettings::setSidebarWidth(int width)
{
    setIntValue(QLatin1String(kSidebarWidth), qBound(kSidebarMinWidth, width, kSidebarMaxWidth));
}

QString AppSettings::sidebarTabId() const
{
    QSettings settings;
    if (settings.contains(QLatin1String(kSidebarTabId))) {
        const QString id = settings.value(QLatin1String(kSidebarTabId)).toString();
        if (id == QLatin1String("file") || id == QLatin1String("tunnel")) {
            return id;
        }
        return QStringLiteral("file");
    }

    // Migrate legacy 3-tab indices [Shell=0, File=1, Tunnel=2] → file/tunnel.
    if (settings.contains(QLatin1String(kSidebarTabIndexLegacy))) {
        const int oldIndex = settings.value(QLatin1String(kSidebarTabIndexLegacy), 0).toInt();
        const QString migrated =
            (oldIndex >= 2) ? QStringLiteral("tunnel") : QStringLiteral("file");
        settings.setValue(QLatin1String(kSidebarTabId), migrated);
        settings.remove(QLatin1String(kSidebarTabIndexLegacy));
        return migrated;
    }
    return QStringLiteral("file");
}

void AppSettings::setSidebarTabId(const QString &tabId)
{
    const QString id =
        (tabId == QLatin1String("tunnel")) ? QStringLiteral("tunnel") : QStringLiteral("file");
    setStringValue(QLatin1String(kSidebarTabId), id);
    QSettings settings;
    settings.remove(QLatin1String(kSidebarTabIndexLegacy));
}

QKeySequence AppSettings::shortcut(const QString &actionId) const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    const QVariant stored = settings.value(actionId);
    settings.endGroup();

    if (stored.isValid() && !stored.toString().isEmpty()) {
        return QKeySequence(stored.toString(), QKeySequence::PortableText);
    }
    return defaultShortcut(actionId);
}

void AppSettings::setShortcut(const QString &actionId, const QKeySequence &sequence)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    settings.setValue(actionId, sequence.toString(QKeySequence::PortableText));
    settings.endGroup();
}

void AppSettings::resetShortcutsToDefaults()
{
    QSettings settings;
    settings.remove(QStringLiteral("shortcuts"));
}

QStringList AppSettings::shortcutActionIds()
{
    QStringList ids;
    ids.reserve(static_cast<int>(std::size(kShortcutDefs)));
    for (const ShortcutDef &def : kShortcutDefs) {
        ids.append(QLatin1String(def.id));
    }
    return ids;
}

QKeySequence AppSettings::defaultShortcut(const QString &actionId)
{
#ifdef Q_OS_MACOS
    // Use portable sequences so defaults work with QCoreApplication-only (easy-ssh-native).
    // QKeySequence::StandardKey requires QGuiApplication.
    if (actionId == QLatin1String("terminal.copy")) {
        return QKeySequence(QStringLiteral("Meta+C"), QKeySequence::PortableText);
    }
    if (actionId == QLatin1String("terminal.paste")) {
        return QKeySequence(QStringLiteral("Meta+V"), QKeySequence::PortableText);
    }
#endif
    if (const ShortcutDef *def = findShortcutDef(actionId)) {
        return QKeySequence(QLatin1String(def->sequence));
    }
    return {};
}

QString AppSettings::shortcutLabel(const QString &actionId)
{
    if (const ShortcutDef *def = findShortcutDef(actionId)) {
        return QCoreApplication::translate("AppSettings", def->label);
    }
    return actionId;
}

QString AppSettings::shortcutGroup(const QString &actionId)
{
    if (const ShortcutDef *def = findShortcutDef(actionId)) {
        return QCoreApplication::translate("AppSettings", def->group);
    }
    return {};
}

void AppSettings::notifyChanged()
{
    QSettings().sync();
    emit settingsChanged();
}
