/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "EasySshRuntime.h"

#include "core/util/Logging.h"

#include <QCoreApplication>

#include <mutex>

namespace {

std::once_flag g_startOnce;
QCoreApplication *g_app = nullptr;
int g_argc = 1;
char g_arg0[] = "easy-ssh-native";
char *g_argv[] = {g_arg0, nullptr};

} // namespace

@implementation EasySshRuntime

+ (void)start
{
    std::call_once(g_startOnce, [] {
        if (QCoreApplication::instance() != nullptr) {
            g_app = QCoreApplication::instance();
            return;
        }
        g_app = new QCoreApplication(g_argc, g_argv);
        QCoreApplication::setOrganizationName(QStringLiteral("easy-ssh"));
        QCoreApplication::setApplicationName(QStringLiteral("easy-ssh-native"));
        QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
        initLogging();
    });
}

+ (BOOL)isRunning
{
    return QCoreApplication::instance() != nullptr;
}

+ (void)shutdown
{
    // Intentionally no-op while the process lives; QCoreApplication must outlive workers.
}

@end
