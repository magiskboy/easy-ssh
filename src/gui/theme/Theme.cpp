// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
// SPDX-FileCopyrightText: Copyright (C) 2024 Beat Reichenbach (qt-themes Theme schema)
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Theme color schema adapted from https://github.com/beatreichenbach/qt-themes
// (MIT License).

#include "gui/theme/Theme.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace
{

bool requireColor(const QJsonObject &obj, const char *key, QColor *out, QString *error)
{
    const QJsonValue value = obj.value(QLatin1String(key));
    if (!value.isString()) {
        if (error) {
            *error = QStringLiteral("Missing or invalid color key '%1'").arg(QLatin1String(key));
        }
        return false;
    }
    const QColor color(value.toString());
    if (!color.isValid()) {
        if (error) {
            *error = QStringLiteral("Invalid color for '%1': %2")
                         .arg(QLatin1String(key), value.toString());
        }
        return false;
    }
    *out = color;
    return true;
}

} // namespace

bool Theme::isDark() const
{
    return text.value() > base.value();
}

bool Theme::loadFromJsonFile(const QString &path, Theme *out, QString *error)
{
    if (!out) {
        if (error) {
            *error = QStringLiteral("Null theme output");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open theme file: %1").arg(path);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid JSON in %1: %2").arg(path, parseError.errorString());
        }
        return false;
    }

    const QJsonObject obj = doc.object();
    Theme theme;

#define LOAD_COLOR(field)                                                                          \
    if (!requireColor(obj, #field, &theme.field, error)) {                                         \
        return false;                                                                              \
    }

    LOAD_COLOR(primary)
    LOAD_COLOR(secondary)
    LOAD_COLOR(magenta)
    LOAD_COLOR(red)
    LOAD_COLOR(orange)
    LOAD_COLOR(yellow)
    LOAD_COLOR(green)
    LOAD_COLOR(cyan)
    LOAD_COLOR(blue)
    LOAD_COLOR(text)
    LOAD_COLOR(subtext1)
    LOAD_COLOR(subtext0)
    LOAD_COLOR(overlay2)
    LOAD_COLOR(overlay1)
    LOAD_COLOR(overlay0)
    LOAD_COLOR(surface2)
    LOAD_COLOR(surface1)
    LOAD_COLOR(surface0)
    LOAD_COLOR(base)
    LOAD_COLOR(mantle)
    LOAD_COLOR(crust)

#undef LOAD_COLOR

    *out = theme;
    return true;
}
