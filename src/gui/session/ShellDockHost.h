/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QHash>
#include <QUuid>
#include <QWidget>

class QLabel;
class QMenu;
class QMimeData;
class QVBoxLayout;

namespace ads
{
class CDockManager;
class CDockWidget;
} // namespace ads

class ShellDockHost final : public QWidget
{
    Q_OBJECT

public:
    static constexpr const char *kShellMimeType = "application/x-easy-ssh-shell-id";

    explicit ShellDockHost(QWidget *parent = nullptr);
    ~ShellDockHost() override;

    QWidget *termHolder() const { return m_termHolder; }

    bool pinShell(const QUuid &shellId,
                  const QString &title,
                  QWidget *term,
                  int dockArea = /* ads::CenterDockWidgetArea */ 0x10,
                  const QUuid &relativeTo = {});
    bool unpinShell(const QUuid &shellId);
    bool focusShell(const QUuid &shellId);
    bool isPinned(const QUuid &shellId) const;
    QList<QUuid> pinnedShellIds() const;
    /// Pinned shells that are not floating OS windows (smart-layout targets).
    QList<QUuid> dockedShellIds() const;
    QUuid focusedShellId() const;
    void clearLayout();
    void setLayoutActive(bool active);
    void setShellTitle(const QUuid &shellId, const QString &title);

signals:
    void shellFocused(const QUuid &shellId);
    void dropShellRequested(const QUuid &shellId, int dockArea);
    /// Emitted while building the dock-tab context menu; callers prepend actions.
    void shellTabContextMenuRequested(const QUuid &shellId, QMenu *menu);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    ads::CDockWidget *dockForShell(const QUuid &shellId) const;
    QUuid shellIdForDock(ads::CDockWidget *dock) const;
    void updateEmptyState();
    int hitTestDockArea(const QPoint &pos) const;
    bool acceptShellDrag(const QMimeData *mime) const;

    ads::CDockManager *m_manager = nullptr;
    QWidget *m_termHolder = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QVBoxLayout *m_root = nullptr;
    QHash<QUuid, ads::CDockWidget *> m_docks;
    bool m_layoutActive = true;
};
