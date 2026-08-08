// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TerminalDockHost.h"

#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>

TerminalDockHost::TerminalDockHost(QWidget *parent) : QWidget(parent)
{
    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(0);

    m_termHolder = new QWidget(this);
    m_termHolder->hide();

    m_emptyLabel =
        new QLabel(tr("No open terminals.\nUse Terminal → New Terminal to open one."), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);

    m_manager = new ads::CDockManager(this);
    m_root->addWidget(m_emptyLabel, 0);
    m_root->addWidget(m_manager, 1);

    connect(m_manager,
            &ads::CDockManager::focusedDockWidgetChanged,
            this,
            [this](ads::CDockWidget * /*old*/, ads::CDockWidget *now) {
                const QUuid id = terminalIdForDock(now);
                if (!id.isNull() && m_docks.contains(id)) {
                    emit terminalFocused(id);
                }
            });

    updateEmptyState();
}

TerminalDockHost::~TerminalDockHost()
{
    clearLayout();
}

bool TerminalDockHost::pinTerminal(const QUuid &terminalId,
                                   const QString &title,
                                   QWidget *term,
                                   int dockArea,
                                   const QUuid &relativeTo)
{
    if (terminalId.isNull() || !term || !m_manager) {
        return false;
    }
    if (m_docks.contains(terminalId)) {
        return focusTerminal(terminalId);
    }

    auto *dock = m_manager->createDockWidget(title.isEmpty() ? tr("Terminal") : title);
    dock->setObjectName(terminalId.toString(QUuid::WithoutBraces));
    dock->setFeature(ads::CDockWidget::CustomCloseHandling, true);
    dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    dock->setWidget(term);

    connect(dock, &ads::CDockWidget::closeRequested, this, [this, terminalId]() {
        emit terminalCloseRequested(terminalId);
    });

    const auto area =
        static_cast<ads::DockWidgetArea>(dockArea == 0 ? ads::CenterDockWidgetArea : dockArea);

    ads::CDockAreaWidget *relativeArea = nullptr;
    if (!relativeTo.isNull()) {
        if (ads::CDockWidget *rel = dockForTerminal(relativeTo)) {
            if (!rel->isFloating()) {
                relativeArea = rel->dockAreaWidget();
            }
        }
    }

    m_manager->addDockWidget(area, dock, relativeArea);
    m_docks.insert(terminalId, dock);
    if (ads::CDockWidgetTab *tab = dock->tabWidget()) {
        tab->installEventFilter(this);
    }
    updateEmptyState();
    dock->setAsCurrentTab();
    term->setFocus(Qt::OtherFocusReason);
    return true;
}

bool TerminalDockHost::eventFilter(QObject *watched, QEvent *event)
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

    auto *ce = static_cast<QContextMenuEvent *>(event);
    QMenu menu(tab);
    tab->buildContextMenu(&menu);

    const QUuid terminalId = terminalIdForDock(tab->dockWidget());
    if (!terminalId.isNull()) {
        menu.addSeparator();
        menu.addAction(tr("Rename…"), this, [this, terminalId]() {
            emit terminalRenameRequested(terminalId);
        });
        menu.exec(ce->globalPos());
        ce->accept();
        return true;
    }

    const QString toolId = toolIdForDock(tab->dockWidget());
    if (!toolId.isEmpty()) {
        emit toolContextMenuAboutToShow(toolId, &menu);
        menu.exec(ce->globalPos());
        ce->accept();
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

bool TerminalDockHost::unpinTerminal(const QUuid &terminalId)
{
    ads::CDockWidget *dock = dockForTerminal(terminalId);
    if (!dock) {
        return false;
    }

    QWidget *term = dock->takeWidget();
    if (term) {
        term->setParent(m_termHolder);
        term->hide();
    }

    m_docks.remove(terminalId);
    m_manager->removeDockWidget(dock);
    dock->deleteLater();
    updateEmptyState();
    return true;
}

bool TerminalDockHost::focusTerminal(const QUuid &terminalId)
{
    ads::CDockWidget *dock = dockForTerminal(terminalId);
    if (!dock) {
        return false;
    }
    dock->toggleView(true);
    dock->setAsCurrentTab();
    if (QWidget *w = dock->widget()) {
        w->setFocus(Qt::OtherFocusReason);
    }
    emit terminalFocused(terminalId);
    return true;
}

bool TerminalDockHost::isPinned(const QUuid &terminalId) const
{
    return m_docks.contains(terminalId);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool TerminalDockHost::pinTool(const QString &toolId,
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

    // Prefer a full-size tab beside existing terminals (not a split pane).
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
    if (ads::CDockWidgetTab *tab = dock->tabWidget()) {
        tab->installEventFilter(this);
    }
    updateEmptyState();
    dock->setAsCurrentTab();
    widget->setFocus(Qt::OtherFocusReason);
    return true;
}

bool TerminalDockHost::unpinTool(const QString &toolId)
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

bool TerminalDockHost::focusTool(const QString &toolId)
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

bool TerminalDockHost::isToolPinned(const QString &toolId) const
{
    return m_tools.contains(toolId);
}

QList<QUuid> TerminalDockHost::pinnedTerminalIds() const
{
    return m_docks.keys();
}

QStringList TerminalDockHost::pinnedToolIds() const
{
    return m_tools.keys();
}

QList<QUuid> TerminalDockHost::dockedTerminalIds() const
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

QUuid TerminalDockHost::focusedTerminalId() const
{
    if (!m_manager) {
        return {};
    }
    return terminalIdForDock(m_manager->focusedDockWidget());
}

QString TerminalDockHost::focusedToolId() const
{
    if (!m_manager) {
        return {};
    }
    return toolIdForDock(m_manager->focusedDockWidget());
}

void TerminalDockHost::clearLayout()
{
    const QList<QString> toolIds = m_tools.keys();
    for (const QString &id : toolIds) {
        unpinTool(id);
    }
    const QList<QUuid> ids = m_docks.keys();
    for (const QUuid &id : ids) {
        unpinTerminal(id);
    }
}

void TerminalDockHost::setLayoutActive(bool active)
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

void TerminalDockHost::setTerminalTitle(const QUuid &terminalId, const QString &title)
{
    if (ads::CDockWidget *dock = dockForTerminal(terminalId)) {
        dock->setWindowTitle(title);
    }
}

QByteArray TerminalDockHost::saveLayout() const
{
    if (!m_manager || !hasAnyDocks()) {
        return {};
    }
    return m_manager->saveState();
}

bool TerminalDockHost::restoreLayout(const QByteArray &state)
{
    if (!m_manager || state.isEmpty()) {
        return false;
    }
    return m_manager->restoreState(state);
}

ads::CDockWidget *TerminalDockHost::dockForTerminal(const QUuid &terminalId) const
{
    return m_docks.value(terminalId, nullptr);
}

ads::CDockWidget *TerminalDockHost::dockForTool(const QString &toolId) const
{
    return m_tools.value(toolId, nullptr);
}

QUuid TerminalDockHost::terminalIdForDock(ads::CDockWidget *dock) const
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

QString TerminalDockHost::toolIdForDock(ads::CDockWidget *dock) const
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

void TerminalDockHost::updateEmptyState()
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

bool TerminalDockHost::hasAnyDocks() const
{
    return !m_docks.isEmpty() || !m_tools.isEmpty();
}
