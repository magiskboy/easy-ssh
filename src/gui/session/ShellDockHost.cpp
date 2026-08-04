// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ShellDockHost.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QVBoxLayout>

#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>

namespace
{
constexpr int kEdgeHitFraction = 4; // outer 1/4 → edge area
}

ShellDockHost::ShellDockHost(QWidget *parent) : QWidget(parent)
{
    setAcceptDrops(true);

    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(0);

    m_termHolder = new QWidget(this);
    m_termHolder->hide();

    m_emptyLabel =
        new QLabel(tr("No shell pinned.\nSelect a shell in the sidebar or drop one here."), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);

    m_manager = new ads::CDockManager(this);
    m_root->addWidget(m_emptyLabel, 0);
    m_root->addWidget(m_manager, 1);

    connect(m_manager,
            &ads::CDockManager::focusedDockWidgetChanged,
            this,
            [this](ads::CDockWidget * /*old*/, ads::CDockWidget *now) {
                const QUuid id = shellIdForDock(now);
                if (!id.isNull() && m_docks.contains(id)) {
                    emit shellFocused(id);
                }
            });

    updateEmptyState();
}

ShellDockHost::~ShellDockHost()
{
    clearLayout();
}

bool ShellDockHost::pinShell(const QUuid &shellId,
                             const QString &title,
                             QWidget *term,
                             int dockArea,
                             const QUuid &relativeTo)
{
    if (shellId.isNull() || !term || !m_manager) {
        return false;
    }
    if (m_docks.contains(shellId)) {
        return focusShell(shellId);
    }

    auto *dock = m_manager->createDockWidget(title.isEmpty() ? tr("Shell") : title);
    dock->setObjectName(shellId.toString(QUuid::WithoutBraces));
    dock->setFeature(ads::CDockWidget::CustomCloseHandling, true);
    dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    dock->setWidget(term);

    connect(
        dock, &ads::CDockWidget::closeRequested, this, [this, shellId]() { unpinShell(shellId); });

    const auto area =
        static_cast<ads::DockWidgetArea>(dockArea == 0 ? ads::CenterDockWidgetArea : dockArea);

    ads::CDockAreaWidget *relativeArea = nullptr;
    if (!relativeTo.isNull()) {
        if (ads::CDockWidget *rel = dockForShell(relativeTo)) {
            if (!rel->isFloating()) {
                relativeArea = rel->dockAreaWidget();
            }
        }
    }

    m_manager->addDockWidget(area, dock, relativeArea);
    m_docks.insert(shellId, dock);
    if (ads::CDockWidgetTab *tab = dock->tabWidget()) {
        tab->installEventFilter(this);
    }
    updateEmptyState();
    dock->setAsCurrentTab();
    term->setFocus(Qt::OtherFocusReason);
    return true;
}

bool ShellDockHost::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::ContextMenu) {
        return QWidget::eventFilter(watched, event);
    }

    auto *tab = qobject_cast<ads::CDockWidgetTab *>(watched);
    if (!tab) {
        return QWidget::eventFilter(watched, event);
    }
    if (tab->dragState() == ads::DraggingFloatingWidget) {
        event->accept();
        return true;
    }

    const QUuid shellId = shellIdForDock(tab->dockWidget());
    if (shellId.isNull()) {
        return QWidget::eventFilter(watched, event);
    }

    auto *ce = static_cast<QContextMenuEvent *>(event);
    QMenu menu(tab);
    tab->buildContextMenu(&menu);
    menu.exec(ce->globalPos());
    ce->accept();
    return true;
}

bool ShellDockHost::unpinShell(const QUuid &shellId)
{
    ads::CDockWidget *dock = dockForShell(shellId);
    if (!dock) {
        return false;
    }

    QWidget *term = dock->takeWidget();
    if (term) {
        term->setParent(m_termHolder);
        term->hide();
    }

    m_docks.remove(shellId);
    m_manager->removeDockWidget(dock);
    dock->deleteLater();
    updateEmptyState();
    return true;
}

bool ShellDockHost::focusShell(const QUuid &shellId)
{
    ads::CDockWidget *dock = dockForShell(shellId);
    if (!dock) {
        return false;
    }
    dock->toggleView(true);
    dock->setAsCurrentTab();
    if (QWidget *w = dock->widget()) {
        w->setFocus(Qt::OtherFocusReason);
    }
    emit shellFocused(shellId);
    return true;
}

bool ShellDockHost::isPinned(const QUuid &shellId) const
{
    return m_docks.contains(shellId);
}

bool ShellDockHost::pinTool(const QString &toolId,
                            const QString &title,
                            QWidget *widget,
                            int dockArea)
{
    if (toolId.isEmpty() || !widget || !m_manager) {
        return false;
    }
    if (m_tools.contains(toolId)) {
        return focusTool(toolId);
    }

    auto *dock = m_manager->createDockWidget(title.isEmpty() ? tr("Tool") : title);
    dock->setObjectName(QStringLiteral("tool:%1").arg(toolId));
    dock->setFeature(ads::CDockWidget::CustomCloseHandling, true);
    dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    dock->setWidget(widget);

    connect(dock, &ads::CDockWidget::closeRequested, this, [this, toolId]() { unpinTool(toolId); });

    // Prefer a full-size tab beside existing shells (not a split pane).
    ads::CDockAreaWidget *targetArea = nullptr;
    if (ads::CDockWidget *focused = m_manager->focusedDockWidget()) {
        if (!focused->isFloating()) {
            targetArea = focused->dockAreaWidget();
        }
    }
    if (!targetArea) {
        for (ads::CDockWidget *shellDock : m_docks) {
            if (shellDock && !shellDock->isFloating()) {
                targetArea = shellDock->dockAreaWidget();
                break;
            }
        }
    }
    if (!targetArea) {
        for (ads::CDockWidget *toolDock : m_tools) {
            if (toolDock && !toolDock->isFloating()) {
                targetArea = toolDock->dockAreaWidget();
                break;
            }
        }
    }

    if (targetArea) {
        m_manager->addDockWidgetTabToArea(dock, targetArea);
    } else {
        const auto area =
            static_cast<ads::DockWidgetArea>(dockArea == 0 ? ads::CenterDockWidgetArea : dockArea);
        m_manager->addDockWidgetTab(area, dock);
    }

    m_tools.insert(toolId, dock);
    updateEmptyState();
    dock->setAsCurrentTab();
    widget->setFocus(Qt::OtherFocusReason);
    return true;
}

bool ShellDockHost::unpinTool(const QString &toolId)
{
    ads::CDockWidget *dock = dockForTool(toolId);
    if (!dock) {
        return false;
    }

    QWidget *widget = dock->takeWidget();
    if (widget) {
        widget->setParent(nullptr);
        widget->hide();
    }

    m_tools.remove(toolId);
    m_manager->removeDockWidget(dock);
    dock->deleteLater();
    updateEmptyState();
    emit toolClosed(toolId);
    return true;
}

bool ShellDockHost::focusTool(const QString &toolId)
{
    ads::CDockWidget *dock = dockForTool(toolId);
    if (!dock) {
        return false;
    }
    dock->toggleView(true);
    dock->setAsCurrentTab();
    if (QWidget *w = dock->widget()) {
        w->setFocus(Qt::OtherFocusReason);
    }
    return true;
}

bool ShellDockHost::isToolPinned(const QString &toolId) const
{
    return m_tools.contains(toolId);
}

QList<QUuid> ShellDockHost::pinnedShellIds() const
{
    return m_docks.keys();
}

QList<QUuid> ShellDockHost::dockedShellIds() const
{
    QList<QUuid> ids;
    ids.reserve(m_docks.size());
    for (auto it = m_docks.cbegin(); it != m_docks.cend(); ++it) {
        if (it.value() && !it.value()->isFloating()) {
            ids.append(it.key());
        }
    }
    return ids;
}

QUuid ShellDockHost::focusedShellId() const
{
    if (!m_manager) {
        return {};
    }
    return shellIdForDock(m_manager->focusedDockWidget());
}

void ShellDockHost::clearLayout()
{
    const QList<QString> toolIds = m_tools.keys();
    for (const QString &id : toolIds) {
        unpinTool(id);
    }
    const QList<QUuid> ids = m_docks.keys();
    for (const QUuid &id : ids) {
        unpinShell(id);
    }
}

void ShellDockHost::setLayoutActive(bool active)
{
    if (!m_manager) {
        m_layoutActive = active;
        return;
    }
    // Leaving a Session tab: hide OS floats (ADS does not do this on hide alone).
    // Returning: must show() the manager again — hideManagerAndFloatingWidgets() sets
    // the Hidden flag, so parenting QTabWidget show alone will not restore it.
    if (m_layoutActive && !active) {
        m_manager->hideManagerAndFloatingWidgets();
    } else if (!m_layoutActive && active && hasAnyDocks()) {
        m_manager->show(); // showEvent → restoreHiddenFloatingWidgets()
    }
    m_layoutActive = active;
}

void ShellDockHost::setShellTitle(const QUuid &shellId, const QString &title)
{
    if (ads::CDockWidget *dock = dockForShell(shellId)) {
        dock->setWindowTitle(title);
    }
}

QByteArray ShellDockHost::saveLayout() const
{
    if (!m_manager || !hasAnyDocks()) {
        return {};
    }
    return m_manager->saveState();
}

bool ShellDockHost::restoreLayout(const QByteArray &state)
{
    if (!m_manager || state.isEmpty()) {
        return false;
    }
    return m_manager->restoreState(state);
}

ads::CDockWidget *ShellDockHost::dockForShell(const QUuid &shellId) const
{
    return m_docks.value(shellId, nullptr);
}

ads::CDockWidget *ShellDockHost::dockForTool(const QString &toolId) const
{
    return m_tools.value(toolId, nullptr);
}

QUuid ShellDockHost::shellIdForDock(ads::CDockWidget *dock) const
{
    if (!dock) {
        return {};
    }
    for (auto it = m_docks.cbegin(); it != m_docks.cend(); ++it) {
        if (it.value() == dock) {
            return it.key();
        }
    }
    const QString name = dock->objectName();
    if (name.startsWith(QLatin1String("tool:"))) {
        return {};
    }
    return QUuid(name);
}

QString ShellDockHost::toolIdForDock(ads::CDockWidget *dock) const
{
    if (!dock) {
        return {};
    }
    for (auto it = m_tools.cbegin(); it != m_tools.cend(); ++it) {
        if (it.value() == dock) {
            return it.key();
        }
    }
    const QString name = dock->objectName();
    constexpr QLatin1String kPrefix("tool:");
    if (name.startsWith(kPrefix)) {
        return name.mid(kPrefix.size());
    }
    return {};
}

void ShellDockHost::updateEmptyState()
{
    const bool empty = !hasAnyDocks();
    if (empty) {
        m_emptyLabel->show();
        m_manager->hide();
    } else {
        m_emptyLabel->hide();
        m_manager->show();
    }
}

bool ShellDockHost::hasAnyDocks() const
{
    return !m_docks.isEmpty() || !m_tools.isEmpty();
}

int ShellDockHost::hitTestDockArea(const QPoint &pos) const
{
    const QRect r = rect();
    if (!r.isValid() || r.width() < 8 || r.height() < 8) {
        return ads::CenterDockWidgetArea;
    }
    const int ew = r.width() / kEdgeHitFraction;
    const int eh = r.height() / kEdgeHitFraction;
    if (pos.x() <= ew) {
        return ads::LeftDockWidgetArea;
    }
    if (pos.x() >= r.width() - ew) {
        return ads::RightDockWidgetArea;
    }
    if (pos.y() <= eh) {
        return ads::TopDockWidgetArea;
    }
    if (pos.y() >= r.height() - eh) {
        return ads::BottomDockWidgetArea;
    }
    return ads::CenterDockWidgetArea;
}

bool ShellDockHost::acceptShellDrag(const QMimeData *mime) const
{
    return mime && mime->hasFormat(QLatin1String(kShellMimeType));
}

void ShellDockHost::dragEnterEvent(QDragEnterEvent *event)
{
    if (acceptShellDrag(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ShellDockHost::dragMoveEvent(QDragMoveEvent *event)
{
    if (acceptShellDrag(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ShellDockHost::dropEvent(QDropEvent *event)
{
    if (!acceptShellDrag(event->mimeData())) {
        event->ignore();
        return;
    }
    const QByteArray raw = event->mimeData()->data(QLatin1String(kShellMimeType));
    const QUuid shellId = QUuid(QString::fromUtf8(raw));
    if (shellId.isNull()) {
        event->ignore();
        return;
    }
    emit dropShellRequested(shellId, hitTestDockArea(event->position().toPoint()));
    event->acceptProposedAction();
}
