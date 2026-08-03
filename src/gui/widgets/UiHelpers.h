/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>

class QAbstractButton;
class QLayout;
class QWidget;

namespace UiHelpers
{

struct TextPrompt
{
    QString title;
    QString label;
    QString text;
};

/// Apply top-level content margins from the style's PM_Layout*Margin metrics.
void applyContentMargins(QLayout *layout, QWidget *styleWidget);

/// Edit + Browse row: zero margins, UiMetrics::tightSpacing, stretch on edit.
QWidget *makeBrowseRow(QWidget *edit, QAbstractButton *browse, QWidget *parent = nullptr);

/// QInputDialog::getText with UiMetrics::inputDialogMinWidth so the title bar fits.
QString getText(QWidget *parent, const TextPrompt &prompt, bool *ok = nullptr);

} // namespace UiHelpers
