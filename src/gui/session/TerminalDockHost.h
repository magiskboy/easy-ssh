/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QHash>
#include <QMenu>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace ads
{
class CDockManager;
class CDockWidget;
} // namespace ads

class TerminalDockHost final : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalDockHost(QWidget *parent = nullptr);
    ~TerminalDockHost() override;

    QWidget *termHolder() const { return m_termHolder; }

    bool pinTerminal(const QUuid &terminalId,
                  const QString &title,
                  QWidget *term,
                  int dockArea = /* ads::CenterDockWidgetArea */ 0x10,
                  const QUuid &relativeTo = {});
    bool unpinTerminal(const QUuid &terminalId);
    bool focusTerminal(const QUuid &terminalId);
    bool isPinned(const QUuid &terminalId) const;

    /// Non-shell tool pane (e.g. process explorer). @p toolId is stable per session page.
    bool pinTool(const QString &toolId,
                 const QString &title,
                 QWidget *widget,
                 int dockArea = /* ads::CenterDockWidgetArea */ 0x10);
    bool unpinTool(const QString &toolId);
    bool focusTool(const QString &toolId);
    bool isToolPinned(const QString &toolId) const;

    QList<QUuid> pinnedTerminalIds() const;
    QStringList pinnedToolIds() const;
    /// Pinned terminals that are not floating OS windows (smart-layout targets).
    QList<QUuid> dockedTerminalIds() const;
    QUuid focusedTerminalId() const;
    QString focusedToolId() const;
    void clearLayout();
    void setLayoutActive(bool active);
    void setTerminalTitle(const QUuid &terminalId, const QString &title);

    QByteArray saveLayout() const;
    bool restoreLayout(const QByteArray &state);

signals:
    void terminalFocused(const QUuid &terminalId);
    /// Dock close button: host should terminate the shell (Session::closeTerminal).
    void terminalCloseRequested(const QUuid &terminalId);
    void terminalRenameRequested(const QUuid &terminalId);
    void toolClosed(const QString &toolId);
    /// Emitted while building a tool dock-tab context menu; listeners may append actions.
    void toolContextMenuAboutToShow(const QString &toolId, QMenu *menu);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    ads::CDockWidget *dockForTerminal(const QUuid &terminalId) const;
    ads::CDockWidget *dockForTool(const QString &toolId) const;
    QUuid terminalIdForDock(ads::CDockWidget *dock) const;
    QString toolIdForDock(ads::CDockWidget *dock) const;
    void updateEmptyState();
    bool hasAnyDocks() const;

    ads::CDockManager *m_manager = nullptr;
    QWidget *m_termHolder = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QVBoxLayout *m_root = nullptr;
    QHash<QUuid, ads::CDockWidget *> m_docks;
    QHash<QString, ads::CDockWidget *> m_tools;
    bool m_layoutActive = true;
};
