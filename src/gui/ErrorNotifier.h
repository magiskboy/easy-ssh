/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>
#include <functional>

class QWidget;

/// Central place for user-visible failures and status updates.
///
/// Consistency rules:
/// - status(): status bar only (guidance, progress results, background sync)
/// - notify(): status bar + modal dialog (blocking problems, credential failures)
/// - QMessageBox::question: keep for destructive confirms
/// - In-dialog QMessageBox::warning: keep for modal form validation
class ErrorNotifier
{
    Q_GADGET

public:
    enum class Level
    {
        Status,
        Success,
        Warning,
        Error,
    };
    Q_ENUM(Level)

    static void setStatusSink(std::function<void(const QString &, Level)> sink);
    static void status(const QString &message, Level level = Level::Status);
    static void notify(QWidget *parent,
                       const QString &title,
                       const QString &message,
                       Level level = Level::Warning);
};
