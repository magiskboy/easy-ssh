// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/settings/AppSettings.h"
#include "core/util/Logging.h"
#include "gui/MainWindow.h"
#include "gui/terminal/QTermWidgetResources.h"
#include "gui/theme/ThemeManager.h"

#include "core/fs/SftpTypes.h"
#include "core/ssh/SshWorker.h"
#include "core/tunnel/Tunnel.h"

#include <DockManager.h>

#include <QApplication>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QVector>

namespace
{
void configureAds()
{
    ads::CDockManager::setConfigFlags(ads::CDockManager::DefaultNonOpaqueConfig);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaCloseButtonClosesTab, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::ActiveTabHasCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::EqualSplitOnInsertion, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideFeatureEnabled, false);
}
} // namespace

int main(int argc, char *argv[])
{
    // Cross-platform look; call before QApplication per Qt docs.
    QApplication::setStyle(QStringLiteral("Fusion"));

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("easy-ssh"));
    QApplication::setApplicationName(QStringLiteral("easy-ssh"));
    QApplication::setApplicationDisplayName(QStringLiteral("Easy SSH"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setOrganizationDomain(QStringLiteral("github.com/magiskboy/easy-ssh"));
    QApplication::setDesktopFileName(QStringLiteral("io.github.magiskboy.easy-ssh"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app-256.png")));

    // Keep the process alive when the main window is hidden to the system tray.
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        QApplication::setQuitOnLastWindowClosed(false);
    }

    ThemeManager::applyFromSettings();

    configureAds();

    initLogging();
    qCWarning(lcApp) << "Starting Easy SSH" << QApplication::applicationVersion()
                     << "log:" << logFilePath();

    registerQTermWidgetResources();

    qRegisterMetaType<RemoteEntry>("RemoteEntry");
    qRegisterMetaType<QVector<RemoteEntry>>("QVector<RemoteEntry>");
    qRegisterMetaType<TunnelDefinition>("TunnelDefinition");
    qRegisterMetaType<SshWorker::HostKeyPrompt>("SshWorker::HostKeyPrompt");

    MainWindow window;
    const bool startHidden =
        AppSettings::instance().startInTray() && QSystemTrayIcon::isSystemTrayAvailable();
    if (!startHidden) {
        window.show();
    }

    return app.exec();
}
