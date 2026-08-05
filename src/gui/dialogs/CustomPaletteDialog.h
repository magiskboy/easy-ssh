/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/theme/Theme.h"

#include <QDialog>
#include <QString>

class QFormLayout;
class QPushButton;

class CustomPaletteDialog final : public QDialog
{
    Q_OBJECT

public:
    /// Opens with @p initial as the editable theme. On accept, returns the edited Theme.
    explicit CustomPaletteDialog(const Theme &initial, QWidget *parent = nullptr);

    Theme theme() const { return m_theme; }

private:
    void addColorRow(QFormLayout *form, const QString &label, QColor Theme::*field);
    void updateSwatch(QPushButton *button, const QColor &color);
    void pickColor(QColor Theme::*field, QPushButton *button);

    Theme m_theme;
};
