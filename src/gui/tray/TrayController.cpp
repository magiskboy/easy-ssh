// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TrayController.h"

#include <QAction>
#include <QGuiApplication>
#include <QHash>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

namespace
{
QColor colorForKind(TrayStatusKind kind)
{
    switch (kind) {
    case TrayStatusKind::Connecting:
        return QColor(QStringLiteral("#f9a825"));
    case TrayStatusKind::Connected:
        return QColor(QStringLiteral("#43a047"));
    case TrayStatusKind::Warning:
        return QColor(QStringLiteral("#e53935"));
    case TrayStatusKind::Idle:
    default:
        return {};
    }
}
} // namespace

TrayController::TrayController(QObject *parent) : QObject(parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_menu = new QMenu();
    auto *showAction = m_menu->addAction(tr("&Show"));
    connect(showAction, &QAction::triggered, this, &TrayController::restoreRequested);

    m_menu->addSeparator();

    m_sessionsMenu = m_menu->addMenu(tr("&Sessions"));
    m_recentMenu = m_menu->addMenu(tr("&Recent"));

    m_menu->addSeparator();

    auto *quitAction = m_menu->addAction(tr("&Quit"));
    connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);

    connect(m_menu, &QMenu::aboutToShow, this, &TrayController::menusAboutToShow);

    rebuildSessionMenu();
    rebuildRecentMenu();

    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(iconForKind(TrayStatusKind::Idle));
    m_tray->setToolTip(QGuiApplication::applicationDisplayName());
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayController::onActivated);
    m_tray->show();
}

TrayController::~TrayController()
{
    delete m_menu;
    m_menu = nullptr;
    m_sessionsMenu = nullptr;
    m_recentMenu = nullptr;
}

bool TrayController::isAvailable() const
{
    return m_tray != nullptr;
}

void TrayController::setVisible(bool visible)
{
    if (m_tray) {
        m_tray->setVisible(visible);
    }
}

void TrayController::setToolTip(const QString &tip)
{
    if (m_tray) {
        m_tray->setToolTip(tip);
    }
}

void TrayController::setStatusKind(TrayStatusKind kind)
{
    if (m_statusKind == kind) {
        return;
    }
    m_statusKind = kind;
    if (m_tray) {
        m_tray->setIcon(iconForKind(kind));
    }
}

void TrayController::showNotification(const QString &title,
                                      const QString &message,
                                      QSystemTrayIcon::MessageIcon icon)
{
    if (!m_tray || !QSystemTrayIcon::supportsMessages()) {
        return;
    }
    m_tray->showMessage(title, message, icon, 5000);
}

void TrayController::setSessionItems(const QVector<TraySessionItem> &items)
{
    m_sessionItems = items;
    rebuildSessionMenu();
}

void TrayController::setRecentItems(const QVector<TrayRecentItem> &items)
{
    m_recentItems = items;
    rebuildRecentMenu();
}

void TrayController::rebuildSessionMenu()
{
    if (!m_sessionsMenu) {
        return;
    }
    m_sessionsMenu->clear();
    if (m_sessionItems.isEmpty()) {
        auto *empty = m_sessionsMenu->addAction(tr("(None)"));
        empty->setEnabled(false);
        return;
    }
    for (const TraySessionItem &item : m_sessionItems) {
        auto *action = m_sessionsMenu->addAction(item.label);
        const QUuid id = item.connectionId;
        connect(
            action, &QAction::triggered, this, [this, id]() { emit activateSessionRequested(id); });
    }
}

void TrayController::rebuildRecentMenu()
{
    if (!m_recentMenu) {
        return;
    }
    m_recentMenu->clear();
    if (m_recentItems.isEmpty()) {
        auto *empty = m_recentMenu->addAction(tr("(None)"));
        empty->setEnabled(false);
        return;
    }
    for (const TrayRecentItem &item : m_recentItems) {
        auto *action = m_recentMenu->addAction(item.label);
        const QUuid id = item.connectionId;
        connect(action, &QAction::triggered, this, [this, id]() { emit openRecentRequested(id); });
    }
}

QIcon TrayController::iconForKind(TrayStatusKind kind)
{
    // Instance cache via static is awkward for QObject; paint each time is cheap for 256px once
    // cached on the tray. Use a process-wide simple cache keyed by kind.
    static QHash<int, QIcon> cache;
    const int key = static_cast<int>(kind);
    if (cache.contains(key)) {
        return cache.value(key);
    }

    QPixmap base(QStringLiteral(":/icons/app-256.png"));
    if (base.isNull()) {
        return {};
    }
    if (base.width() > 64) {
        base = base.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const QColor dot = colorForKind(kind);
    if (dot.isValid()) {
        QPainter painter(&base);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const int d = qMax(10, base.width() / 4);
        const QRectF r(base.width() - d - 2, base.height() - d - 2, d, d);
        painter.setPen(QPen(Qt::white, 2.0));
        painter.setBrush(dot);
        painter.drawEllipse(r);
    }

    const QIcon icon(base);
    cache.insert(key, icon);
    return icon;
}

void TrayController::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        emit restoreRequested();
        break;
    default:
        break;
    }
}
