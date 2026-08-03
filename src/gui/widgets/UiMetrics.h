/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

/// Shared spacing / size tokens for native-first Qt chrome and content dialogs.
namespace UiMetrics
{

constexpr int chromeMargin = 0;
constexpr int chromeSpacing = 0;

constexpr int tightSpacing = 4;
constexpr int relatedSpacing = 8;
constexpr int sectionSpacing = 12;
constexpr int overlayMargin = 16;

constexpr int categorySidebarWidth = 160;
constexpr int categoryRowHeight = 28;

/// Floor width for modeless content dialogs (forms stay readable when resized).
constexpr int dialogMinWidth = 520;

/// Floor width for compact prompts (QInputDialog) so the title bar is not clipped.
constexpr int inputDialogMinWidth = 360;

/// Welcome screen Recent/Keyboard column — balanced against the title cluster.
constexpr int welcomeContentMinWidth = 320;
constexpr int welcomeContentMaxWidth = 480;

} // namespace UiMetrics
