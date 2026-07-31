// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SessionTabWidget.h"

#include "SessionPage.h"
#include "core/session/Session.h"
#include "core/session/SessionManager.h"
#include "gui/connection/WelcomeWidget.h"
#include "gui/models/ConnectionModel.h"

#include <QAction>
#include <QMenu>
#include <QTabBar>

SessionTabWidget::SessionTabWidget(QWidget *parent) : QTabWidget(parent)
{
    setDocumentMode(true);
    setTabsClosable(true);
    setMovable(true);
    setUsesScrollButtons(true);

    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, &QTabWidget::tabCloseRequested, this, &SessionTabWidget::onTabCloseRequested);
    connect(this, &QTabWidget::currentChanged, this, &SessionTabWidget::onCurrentChanged);
    connect(
        tabBar(), &QWidget::customContextMenuRequested, this, &SessionTabWidget::onTabContextMenu);

    ensureWelcomeTab();
}

void SessionTabWidget::setConnectionModel(ConnectionModel *model)
{
    m_connectionModel = model;
    if (auto *welcome = welcomeWidget()) {
        welcome->setConnectionModel(m_connectionModel);
    }
}

void SessionTabWidget::setSessionManager(SessionManager *manager)
{
    m_sessionManager = manager;
}

void SessionTabWidget::refreshWelcome()
{
    if (auto *welcome = welcomeWidget()) {
        welcome->refresh();
    }
}

void SessionTabWidget::openSshSession(const Connection &connection,
                                      const SessionCredentials &credentials)
{
    if (!m_sessionManager) {
        return;
    }

    const int existing = indexForConnection(connection.id);
    if (existing >= 0) {
        setCurrentIndex(existing);
        if (Session *session = m_sessionManager->get(connection.id)) {
            session->setConnection(connection);
            session->setCredentials(credentials);
            if (session->state() == SessionState::Disconnected ||
                session->state() == SessionState::Failed) {
                session->reconnect();
            }
        }
        return;
    }

    removeWelcomeTabIfPresent();

    Session *session = m_sessionManager->open(connection, credentials);
    auto *page = new SessionPage(session, this);
    m_pagesByConnection.insert(connection.id, page);

    connect(page, &SessionPage::statusMessage, this, &SessionTabWidget::statusMessage);
    connect(page, &SessionPage::closeRequested, this, [this, page]() {
        const int index = indexOf(page);
        if (index >= 0) {
            onTabCloseRequested(index);
        }
    });
    connect(page, &SessionPage::editRequested, this, [this, session]() {
        emit editConnectionRequested(session->connectionId());
    });
    connect(page, &SessionPage::reconnectRequested, this, [this, session]() {
        emit openConnectionRequested(session->connectionId());
    });
    connect(session, &Session::stateChanged, this, [this, page](SessionState) {
        updateTabPresentation(page);
    });

    const int index = addTab(page, session->displayName());
    setCurrentIndex(index);
    session->connectTransport();
    updateTabPresentation(page);
    emit sessionOpened(session->displayName());
}

void SessionTabWidget::disconnectCurrentSession()
{
    if (Session *session = activeSession()) {
        session->disconnectTransport();
    }
}

void SessionTabWidget::reconnectCurrentSession()
{
    Session *session = activeSession();
    if (!session) {
        return;
    }
    // Re-read secrets from the keychain so edits to password/passphrase apply.
    if (session->state() == SessionState::Connected ||
        session->state() == SessionState::Connecting) {
        session->disconnectTransport();
    }
    emit openConnectionRequested(session->connectionId());
}

void SessionTabWidget::closeCurrentSession()
{
    const int index = currentIndex();
    if (index < 0 || isWelcomeTab(index)) {
        return;
    }
    onTabCloseRequested(index);
}

void SessionTabWidget::nextSession()
{
    if (count() <= 1) {
        return;
    }
    setCurrentIndex((currentIndex() + 1) % count());
}

void SessionTabWidget::previousSession()
{
    if (count() <= 1) {
        return;
    }
    const int prev = currentIndex() - 1;
    setCurrentIndex(prev < 0 ? count() - 1 : prev);
}

void SessionTabWidget::applySettingsToAllSessions()
{
    for (SessionPage *page : allSessionPages()) {
        page->applySettings();
    }
}

SessionPage *SessionTabWidget::activeSessionPage() const
{
    return pageAt(currentIndex());
}

Session *SessionTabWidget::activeSession() const
{
    if (SessionPage *page = activeSessionPage()) {
        return page->session();
    }
    return nullptr;
}

QList<SessionPage *> SessionTabWidget::allSessionPages() const
{
    return m_pagesByConnection.values();
}

void SessionTabWidget::onTabCloseRequested(int index)
{
    if (isWelcomeTab(index)) {
        return;
    }
    SessionPage *page = pageAt(index);
    if (!page) {
        return;
    }
    page->setLayoutActive(false);
    const QString name = page->session() ? page->session()->displayName() : QString();
    const QUuid id = page->session() ? page->session()->connectionId() : QUuid();
    m_pagesByConnection.remove(id);
    removeTab(index);
    page->deleteLater();
    if (m_sessionManager && !id.isNull()) {
        m_sessionManager->close(id);
    }
    emit sessionClosed(name);
    if (count() == 0) {
        ensureWelcomeTab();
    }
}

void SessionTabWidget::onCurrentChanged(int index)
{
    SessionPage *active = pageAt(index);
    for (SessionPage *page : m_pagesByConnection) {
        page->setLayoutActive(page == active);
    }
    if (active && active->session() && m_sessionManager) {
        m_sessionManager->setActive(active->session()->connectionId());
        emit activeSessionChanged(active->session()->displayName());
    } else {
        emit activeSessionChanged(QString());
    }
}

void SessionTabWidget::onTabContextMenu(const QPoint &pos)
{
    const int index = tabBar()->tabAt(pos);
    if (index < 0 || isWelcomeTab(index)) {
        return;
    }
    SessionPage *page = pageAt(index);
    if (!page || !page->session()) {
        return;
    }
    Session *session = page->session();

    QMenu menu(this);
    menu.addAction(tr("New shell"), session, [session]() { session->newShell(); });
    menu.addSeparator();
    if (session->state() == SessionState::Connected) {
        menu.addAction(tr("Disconnect"), session, &Session::disconnectTransport);
    } else {
        menu.addAction(tr("Reconnect"), this, [this, session]() {
            emit openConnectionRequested(session->connectionId());
        });
    }
    menu.addAction(tr("Close"), this, [this, index]() { onTabCloseRequested(index); });
    menu.addSeparator();
    menu.addAction(tr("Edit connection…"), this, [this, session]() {
        emit editConnectionRequested(session->connectionId());
    });
    menu.addAction(tr("Duplicate connection…"), this, [this, session]() {
        if (!m_connectionModel) {
            return;
        }
        m_connectionModel->duplicate(session->connectionId());
        emit statusMessage(tr("Duplicated connection"), ErrorNotifier::Level::Success);
    });
    menu.addAction(tr("Delete connection…"), this, [this, session]() {
        emit deleteConnectionRequested(session->connectionId());
    });
    menu.exec(tabBar()->mapToGlobal(pos));
}

void SessionTabWidget::ensureWelcomeTab()
{
    if (m_welcomeIndex >= 0) {
        return;
    }
    auto *welcome = new WelcomeWidget(this);
    welcome->setConnectionModel(m_connectionModel);
    connect(welcome,
            &WelcomeWidget::openConnectionRequested,
            this,
            &SessionTabWidget::openConnectionRequested);
    connect(welcome,
            &WelcomeWidget::createConnectionRequested,
            this,
            &SessionTabWidget::createConnectionRequested);
    connect(welcome,
            &WelcomeWidget::showConnectionsRequested,
            this,
            &SessionTabWidget::showConnectionsRequested);
    m_welcomeIndex = addTab(welcome, tr("Welcome"));
    setTabsClosable(false);
    tabBar()->setTabButton(m_welcomeIndex, QTabBar::RightSide, nullptr);
    setTabsClosable(true);
}

void SessionTabWidget::removeWelcomeTabIfPresent()
{
    if (m_welcomeIndex < 0) {
        return;
    }
    QWidget *w = widget(m_welcomeIndex);
    removeTab(m_welcomeIndex);
    delete w;
    m_welcomeIndex = -1;
}

bool SessionTabWidget::isWelcomeTab(int index) const
{
    return index == m_welcomeIndex;
}

WelcomeWidget *SessionTabWidget::welcomeWidget() const
{
    if (m_welcomeIndex < 0) {
        return nullptr;
    }
    return qobject_cast<WelcomeWidget *>(widget(m_welcomeIndex));
}

SessionPage *SessionTabWidget::pageAt(int index) const
{
    if (index < 0 || isWelcomeTab(index)) {
        return nullptr;
    }
    return qobject_cast<SessionPage *>(widget(index));
}

void SessionTabWidget::updateTabPresentation(SessionPage *page)
{
    if (!page || !page->session()) {
        return;
    }
    const int index = indexOf(page);
    if (index < 0) {
        return;
    }
    setTabText(index, page->session()->displayName());
    QString tip = page->session()->displayName();
    switch (page->session()->state()) {
    case SessionState::Connecting:
        tip += tr(" — Connecting");
        break;
    case SessionState::Connected:
        tip += tr(" — Connected");
        break;
    case SessionState::Disconnected:
        tip += tr(" — Disconnected");
        break;
    case SessionState::Failed:
        tip += tr(" — Failed");
        break;
    }
    setTabToolTip(index, tip);
}

void SessionTabWidget::refreshConnectionPresentation(const QUuid &connectionId)
{
    if (SessionPage *page = m_pagesByConnection.value(connectionId, nullptr)) {
        updateTabPresentation(page);
    }
}

int SessionTabWidget::indexForConnection(const QUuid &connectionId) const
{
    SessionPage *page = m_pagesByConnection.value(connectionId, nullptr);
    return page ? indexOf(page) : -1;
}
