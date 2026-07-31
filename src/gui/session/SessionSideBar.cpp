// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SessionSideBar.h"

#include "ShellListWidget.h"
#include "core/settings/AppSettings.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

SessionSideBar::SessionSideBar(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_placeholder = new QLabel(tr("Open a connection from Connections → List"), this);
    m_placeholder->setWordWrap(true);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setContentsMargins(12, 12, 12, 12);
    root->addWidget(m_placeholder);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setMovable(false);
    m_tabs->setUsesScrollButtons(false);

    m_shellList = new ShellListWidget(m_tabs);
    m_tabs->addTab(m_shellList, tr("Shell"));

    m_fileContainer = new QWidget(m_tabs);
    auto *fileLayout = new QVBoxLayout(m_fileContainer);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    m_tabs->addTab(m_fileContainer, tr("File"));

    m_tunnelContainer = new QWidget(m_tabs);
    auto *tunnelLayout = new QVBoxLayout(m_tunnelContainer);
    tunnelLayout->setContentsMargins(0, 0, 0, 0);
    m_tabs->addTab(m_tunnelContainer, tr("Tunnel"));

    auto *corner = new QWidget(m_tabs);
    auto *cornerLayout = new QHBoxLayout(corner);
    cornerLayout->setContentsMargins(0, 0, 4, 0);
    cornerLayout->setSpacing(0);
    m_newShellAction = new QToolButton(corner);
    m_newShellAction->setText(QStringLiteral("+"));
    m_newShellAction->setToolTip(tr("New shell"));
    m_newShellAction->setAutoRaise(true);
    m_newShellAction->setFixedSize(22, 22);
    m_newShellAction->setEnabled(false);
    connect(m_newShellAction, &QToolButton::clicked, m_shellList, &ShellListWidget::newShell);
    cornerLayout->addWidget(m_newShellAction);
    m_tabs->setCornerWidget(corner, Qt::TopRightCorner);

    connect(m_tabs, &QTabWidget::currentChanged, this, &SessionSideBar::saveTabState);
    // Show + only on Shell tab.
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_newShellAction) {
            m_newShellAction->setVisible(index == 0);
        }
    });

    root->addWidget(m_tabs, 1);
    loadTabState();
    unbindSession();
}

void SessionSideBar::bindSession(Session *session)
{
    const bool has = session != nullptr;
    m_placeholder->setVisible(!has);
    m_tabs->setVisible(has);
    m_shellList->bindSession(session);
    if (m_newShellAction) {
        m_newShellAction->setEnabled(has);
        m_newShellAction->setVisible(has && m_tabs->currentIndex() == 0);
    }
}

void SessionSideBar::unbindSession()
{
    m_shellList->unbindSession();
    m_placeholder->setVisible(true);
    m_tabs->setVisible(false);
    if (m_newShellAction) {
        m_newShellAction->setEnabled(false);
    }
}

void SessionSideBar::loadTabState()
{
    const int index = AppSettings::instance().sidebarTabIndex();
    if (index >= 0 && index < m_tabs->count()) {
        m_tabs->setCurrentIndex(index);
    }
    if (m_newShellAction) {
        m_newShellAction->setVisible(m_tabs->currentIndex() == 0);
    }
}

void SessionSideBar::saveTabState()
{
    if (m_tabs) {
        AppSettings::instance().setSidebarTabIndex(m_tabs->currentIndex());
    }
}
