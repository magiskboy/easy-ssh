/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 * SPDX-FileCopyrightText: Copyright (C) 2024 Beat Reichenbach (qt-themes Theme schema)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Theme color schema adapted from https://github.com/beatreichenbach/qt-themes
 * (MIT License).
 */

#pragma once

#include <QColor>
#include <QString>

struct Theme
{
    QColor primary;
    QColor secondary;

    QColor magenta;
    QColor red;
    QColor orange;
    QColor yellow;
    QColor green;
    QColor cyan;
    QColor blue;

    QColor text;
    QColor subtext1;
    QColor subtext0;
    QColor overlay2;
    QColor overlay1;
    QColor overlay0;
    QColor surface2;
    QColor surface1;
    QColor surface0;
    QColor base;
    QColor mantle;
    QColor crust;

    bool isDark() const;

    /// Load a qt-themes JSON file. Returns false on I/O or schema errors.
    static bool loadFromJsonFile(const QString &path, Theme *out, QString *error = nullptr);
};
