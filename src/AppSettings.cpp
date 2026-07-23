#include "AppSettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QtGlobal>

#include <iterator>

namespace {

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

constexpr auto kAutoReconnect = "session/autoReconnect";

struct ShortcutDef {
    const char *id;
    const char *group;
    const char *label;
    const char *sequence;
};

constexpr ShortcutDef kShortcutDefs[] = {
    {"general.newConnection", "General", "New Connection", "Ctrl+N"},
    {"general.searchConnection", "General", "Search Connection", "Ctrl+F"},
    {"general.settings", "General", "Settings", "Ctrl+,"},
    {"general.shortcuts", "General", "Keyboard Shortcuts", "Ctrl+Shift+,"},
    {"general.about", "General", "About", "F1"},

    {"session.newSession", "Session", "New Session", "Ctrl+T"},
    {"session.closeSession", "Session", "Close Session", "Ctrl+W"},
    {"session.nextTab", "Session", "Next Tab", "Ctrl+Tab"},
    {"session.previousTab", "Session", "Previous Tab", "Ctrl+Shift+Tab"},
    {"session.reconnect", "Session", "Reconnect", "Ctrl+R"},

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

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
}

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

bool AppSettings::autoReconnect() const
{
    return boolValue(QLatin1String(kAutoReconnect), false);
}

void AppSettings::setAutoReconnect(bool enabled)
{
    setBoolValue(QLatin1String(kAutoReconnect), enabled);
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
