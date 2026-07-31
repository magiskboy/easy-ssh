// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "QTermWidgetResources.h"

#include "core/util/Logging.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>

#include <qtermwidget.h>

void registerQTermWidgetResources()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList roots = {
        QDir::cleanPath(appDir + QStringLiteral("/../share/easy-ssh")),
        QDir::cleanPath(appDir + QStringLiteral("/share/easy-ssh")),
#ifdef Q_OS_MAC
        QDir::cleanPath(appDir + QStringLiteral("/../Resources")),
#endif
    };

    int registered = 0;
    for (const QString &root : roots) {
        const QString schemes = root + QStringLiteral("/color-schemes");
        if (!QDir(schemes).exists()) {
            continue;
        }
        QTermWidget::addCustomColorSchemeDir(schemes);
        ++registered;
        qCDebug(lcGui) << "QTermWidget color-schemes dir:" << schemes;
    }

    if (registered == 0) {
        qCWarning(lcGui) << "No bundled QTermWidget color-schemes found under"
                         << roots.join(QStringLiteral(", "));
    }
}
