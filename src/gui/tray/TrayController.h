/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>
#include <QUuid>
#include <QVector>

class QMenu;

enum class TrayStatusKind
{
    Idle,
    Connecting,
    Connected,
    Warning,
};

struct TraySessionItem
{
    QUuid connectionId;
    QString label;
};

struct TrayRecentItem
{
    QUuid connectionId;
    QString label;
};

/// Owns the application system-tray icon (always visible when available).
class TrayController final : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject *parent = nullptr);
    ~TrayController() override;

    bool isAvailable() const;
    void setVisible(bool visible);
    void setToolTip(const QString &tip);
    void setStatusKind(TrayStatusKind kind);
    void showNotification(const QString &title,
                          const QString &message,
                          QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
    void setSessionItems(const QVector<TraySessionItem> &items);
    void setRecentItems(const QVector<TrayRecentItem> &items);

signals:
    void restoreRequested();
    void quitRequested();
    void activateSessionRequested(const QUuid &connectionId);
    void openRecentRequested(const QUuid &connectionId);
    void menusAboutToShow();

private:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void rebuildSessionMenu();
    void rebuildRecentMenu();
    static QIcon iconForKind(TrayStatusKind kind);

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QMenu *m_sessionsMenu = nullptr;
    QMenu *m_recentMenu = nullptr;
    TrayStatusKind m_statusKind = TrayStatusKind::Idle;
    QVector<TraySessionItem> m_sessionItems;
    QVector<TrayRecentItem> m_recentItems;
};
