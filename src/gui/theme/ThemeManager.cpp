// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
// SPDX-FileCopyrightText: Copyright (C) 2024 Beat Reichenbach (qt-themes palette mapping)
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Palette mapping adapted from https://github.com/beatreichenbach/qt-themes
// (MIT License).

#include "gui/theme/ThemeManager.h"

#include "core/settings/AppSettings.h"
#include "core/util/Logging.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPalette>

namespace
{

void updatePalette(QPalette &palette, const Theme &theme)
{
    const QColor highlightedColor = theme.primary;
    const QColor highlightedTextColor = highlightedColor.valueF() > 0.5 ? theme.mantle : theme.text;

    float h = 0.0f;
    float s = 0.0f;
    float v = 0.0f;
    float a = 0.0f;
    theme.text.getHsvF(&h, &s, &v, &a);
    const QColor brightTextColor = QColor::fromHsvF(h, s, 1.0 - v, a);

    if (theme.isDark()) {
        palette.setColor(QPalette::Base, theme.mantle);
        palette.setColor(QPalette::AlternateBase, theme.base);
    } else {
        palette.setColor(QPalette::Base, theme.crust);
        palette.setColor(QPalette::AlternateBase, theme.mantle);
    }
    palette.setColor(QPalette::Window, theme.base);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::PlaceholderText, theme.overlay1);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::Button, theme.base);
    palette.setColor(QPalette::ButtonText, theme.text);
    palette.setColor(QPalette::BrightText, brightTextColor);
    palette.setColor(QPalette::ToolTipBase, theme.mantle);
    palette.setColor(QPalette::ToolTipText, theme.overlay2);

    palette.setColor(QPalette::Highlight, highlightedColor);
    palette.setColor(QPalette::HighlightedText, highlightedTextColor);
    palette.setColor(QPalette::Link, theme.secondary);
    palette.setColor(QPalette::LinkVisited, theme.secondary);

    palette.setColor(QPalette::Light, theme.crust);
    palette.setColor(QPalette::Midlight, theme.mantle);
    palette.setColor(QPalette::Mid, theme.surface0);
    palette.setColor(QPalette::Dark, theme.surface1);
    palette.setColor(QPalette::Shadow, theme.overlay0);

    palette.setColor(QPalette::Inactive, QPalette::Highlight, theme.surface1);
    palette.setColor(QPalette::Inactive, QPalette::Link, theme.surface1);
    palette.setColor(QPalette::Inactive, QPalette::LinkVisited, theme.surface1);

    palette.setColor(QPalette::Disabled, QPalette::WindowText, theme.overlay1);
    palette.setColor(QPalette::Disabled, QPalette::Base, theme.base);
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, theme.base);
    palette.setColor(QPalette::Disabled, QPalette::Text, theme.overlay1);
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, theme.overlay1);
    palette.setColor(QPalette::Disabled, QPalette::Button, theme.base);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, theme.overlay1);
    palette.setColor(QPalette::Disabled, QPalette::BrightText, theme.mantle);

    palette.setColor(QPalette::Disabled, QPalette::Highlight, theme.surface2);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, theme.surface0);
    palette.setColor(QPalette::Disabled, QPalette::Link, theme.surface0);
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, theme.surface0);

    palette.setColor(QPalette::Accent, theme.secondary);
    palette.setColor(QPalette::Inactive, QPalette::Accent, theme.surface1);
    palette.setColor(QPalette::Disabled, QPalette::Accent, theme.surface2);
}

} // namespace

QStringList ThemeManager::themeSearchPaths()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList paths = {
        QDir::cleanPath(appDir + QStringLiteral("/../share/easy-ssh/themes")),
        QDir::cleanPath(appDir + QStringLiteral("/share/easy-ssh/themes")),
#ifdef Q_OS_MAC
        QDir::cleanPath(appDir + QStringLiteral("/../Resources/themes")),
#endif
    };

    const QString env = qEnvironmentVariable("QT_THEMES");
    if (!env.isEmpty()) {
        const QStringList extra = env.split(QDir::listSeparator(), Qt::SkipEmptyParts);
        for (const QString &entry : extra) {
            paths.append(QDir::cleanPath(entry));
        }
    }

    QStringList unique;
    for (const QString &path : paths) {
        if (!path.isEmpty() && !unique.contains(path)) {
            unique.append(path);
        }
    }
    return unique;
}

QStringList ThemeManager::availableThemes()
{
    QStringList ids;
    for (const QString &dirPath : themeSearchPaths()) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            continue;
        }
        const QStringList files =
            dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString &fileName : files) {
            const QString id = QFileInfo(fileName).completeBaseName();
            if (!ids.contains(id)) {
                ids.append(id);
            }
        }
    }
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

std::optional<Theme> ThemeManager::loadTheme(const QString &id)
{
    if (id.isEmpty() || id == QLatin1String(kSystemThemeId) ||
        id == QLatin1String(kCustomThemeId)) {
        return std::nullopt;
    }

    const QString fileName = id + QStringLiteral(".json");
    for (const QString &dirPath : themeSearchPaths()) {
        const QString path = QDir(dirPath).filePath(fileName);
        if (QFileInfo::exists(path)) {
            return loadThemeFile(path);
        }
    }

    qCWarning(lcGui) << "Theme not found:" << id;
    return std::nullopt;
}

std::optional<Theme> ThemeManager::loadThemeFile(const QString &path)
{
    Theme theme;
    QString error;
    if (!Theme::loadFromJsonFile(path, &theme, &error)) {
        qCWarning(lcGui) << "Failed to load theme" << path << ":" << error;
        return std::nullopt;
    }
    return theme;
}

void ThemeManager::applyPalette(const Theme &theme)
{
    QPalette palette;
    updatePalette(palette, theme);
    QApplication::setPalette(palette);
}

void ThemeManager::applySystemPalette()
{
    QApplication::setPalette(QPalette());
}

void ThemeManager::applyFromSettings()
{
    const auto &settings = AppSettings::instance();
    const QString themeId = settings.themeId();

    if (themeId.isEmpty() || themeId == QLatin1String(kSystemThemeId)) {
        applySystemPalette();
        return;
    }

    if (themeId == QLatin1String(kCustomThemeId)) {
        const QString path = settings.customThemePath().trimmed();
        if (path.isEmpty()) {
            qCWarning(lcGui) << "Custom theme selected but path is empty; using system palette";
            applySystemPalette();
            return;
        }
        if (const auto theme = loadThemeFile(path)) {
            applyPalette(*theme);
        } else {
            qCWarning(lcGui) << "Falling back to system palette after custom theme load failure";
            applySystemPalette();
        }
        return;
    }

    if (const auto theme = loadTheme(themeId)) {
        applyPalette(*theme);
    } else {
        qCWarning(lcGui) << "Falling back to system palette; unknown theme" << themeId;
        applySystemPalette();
    }
}

QString ThemeManager::displayName(const QString &id)
{
    if (id == QLatin1String(kSystemThemeId)) {
        return QStringLiteral("System");
    }
    if (id == QLatin1String(kCustomThemeId)) {
        return QStringLiteral("Custom…");
    }

    QStringList parts = id.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    for (QString &part : parts) {
        if (!part.isEmpty()) {
            part[0] = part[0].toUpper();
        }
    }
    return parts.join(QLatin1Char(' '));
}
