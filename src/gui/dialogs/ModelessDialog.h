/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/widgets/UiMetrics.h"

#include <QDialog>

/// Prepare a modeless secondary window that keeps a normal frame on Wayland.
///
/// Per Qt QDialog docs, modeless dialogs use show()/raise()/activateWindow() and
/// may keep a widget parent (see FindDialog example). On Wayland compositors
/// (e.g. GNOME), the default Qt::Dialog window type often ends up without
/// server-side decorations and with incomplete client-side decorations, so the
/// surface looks frameless. Qt::Window requests a normal decorated top-level
/// instead (same class of window as QMainWindow).
///
/// @p minWidth defaults to UiMetrics::dialogMinWidth so form content stays usable;
/// pass 0 to skip (e.g. compact About).
inline void configureModelessDialog(QDialog *dialog, int minWidth = UiMetrics::dialogMinWidth)
{
    dialog->setWindowFlags(Qt::Window);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (minWidth > 0) {
        dialog->setMinimumWidth(minWidth);
    }
}
