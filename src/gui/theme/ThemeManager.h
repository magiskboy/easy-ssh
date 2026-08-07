/*
 * SPDX-FileCopyrightText: Copyright (C) 2024 Beat Reichenbach (qt-themes palette mapping)
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/theme/Theme.h"

#include <QString>
#include <QStringList>
#include <optional>

class ThemeManager
{
public:
    static constexpr auto kSystemThemeId = "system";
    static constexpr auto kCustomThemeId = "custom";
    static constexpr auto kSystemFontMode = "system";
    static constexpr auto kCustomFontMode = "custom";

    /// Directories that may contain `<name>.json` theme files.
    static QStringList themeSearchPaths();

    /// Sorted list of bundled / QT_THEMES theme ids (basename without .json).
    static QStringList availableThemes();

    static std::optional<Theme> loadTheme(const QString &id);
    static std::optional<Theme> loadThemeFile(const QString &path);

    /// App-config path for the in-app custom palette JSON.
    static QString customPalettePath();
    static std::optional<Theme> loadCustomPalette();
    static bool saveCustomPalette(const Theme &theme, QString *error = nullptr);

    /// Resolve a Theme for the Customize dialog: custom file → legacy path →
    /// seedThemeId bundled → first available bundled.
    static std::optional<Theme> resolveCustomPaletteSeed(const QString &seedThemeId = {});

    static void applyPalette(const Theme &theme);
    static void applySystemPalette();

    /// Read AppSettings and apply system / named / custom theme.
    static void applyFromSettings();

    /// Read AppSettings and apply System / Custom UI font.
    static void applyUiFontFromSettings();

    /// Human-readable label for a theme id (e.g. nord → "Nord").
    static QString displayName(const QString &id);
};
