/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 * SPDX-FileCopyrightText: Copyright (C) 2024 Beat Reichenbach (qt-themes palette mapping)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Palette mapping adapted from https://github.com/beatreichenbach/qt-themes
 * (MIT License).
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

    /// Directories that may contain `<name>.json` theme files.
    static QStringList themeSearchPaths();

    /// Sorted list of bundled / QT_THEMES theme ids (basename without .json).
    static QStringList availableThemes();

    static std::optional<Theme> loadTheme(const QString &id);
    static std::optional<Theme> loadThemeFile(const QString &path);

    static void applyPalette(const Theme &theme);
    static void applySystemPalette();

    /// Read AppSettings and apply system / named / custom theme.
    static void applyFromSettings();

    /// Human-readable label for a theme id (e.g. nord → "Nord").
    static QString displayName(const QString &id);
};
