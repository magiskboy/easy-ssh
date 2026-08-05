// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SessionSideBar.h"

#include "core/settings/AppSettings.h"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
constexpr QLatin1String kTabFile("file");
constexpr QLatin1String kTabTunnel("tunnel");
} // namespace

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

    m_fileContainer = new QWidget(m_tabs);
    m_fileContainer->setObjectName(kTabFile);
    auto *fileLayout = new QVBoxLayout(m_fileContainer);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    m_tabs->addTab(m_fileContainer, tr("File"));

    m_tunnelContainer = new QWidget(m_tabs);
    m_tunnelContainer->setObjectName(kTabTunnel);
    auto *tunnelLayout = new QVBoxLayout(m_tunnelContainer);
    tunnelLayout->setContentsMargins(0, 0, 0, 0);
    m_tabs->addTab(m_tunnelContainer, tr("Tunnel"));

    connect(m_tabs, &QTabWidget::currentChanged, this, &SessionSideBar::saveTabState);

    root->addWidget(m_tabs, 1);
    loadTabState();
    unbindSession();
}

void SessionSideBar::bindSession(Session *session)
{
    const bool has = session != nullptr;
    m_placeholder->setVisible(!has);
    m_tabs->setVisible(has);
}

void SessionSideBar::unbindSession()
{
    m_placeholder->setVisible(true);
    m_tabs->setVisible(false);
}

void SessionSideBar::loadTabState()
{
    setCurrentTabId(AppSettings::instance().sidebarTabId());
}

void SessionSideBar::saveTabState()
{
    if (m_tabs) {
        AppSettings::instance().setSidebarTabId(currentTabId());
    }
}

QString SessionSideBar::currentTabId() const
{
    if (!m_tabs || !m_tabs->currentWidget()) {
        return QString(kTabFile);
    }
    const QString id = m_tabs->currentWidget()->objectName();
    return id.isEmpty() ? QString(kTabFile) : id;
}

void SessionSideBar::setCurrentTabId(const QString &tabId)
{
    if (!m_tabs) {
        return;
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->widget(i) && m_tabs->widget(i)->objectName() == tabId) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
    m_tabs->setCurrentIndex(0);
}
