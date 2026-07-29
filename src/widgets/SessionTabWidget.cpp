#include "SessionTabWidget.h"

#include "TerminalSessionWidget.h"
#include "WelcomeWidget.h"

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

void SessionTabWidget::refreshWelcome()
{
    if (auto *welcome = welcomeWidget()) {
        welcome->refresh();
    }
}

void SessionTabWidget::openSshSession(const Connection &connection, const QString &secret)
{
    removeWelcomeTabIfPresent();

    auto *session = new TerminalSessionWidget(this);
    connect(session, &TerminalSessionWidget::statusMessage, this, &SessionTabWidget::statusMessage);
    connect(session, &TerminalSessionWidget::sessionFailed, this, [this, session](const QString &) {
        updateTabPresentation(session);
    });
    connect(session, &TerminalSessionWidget::sessionDisconnected, this, [this, session]() {
        updateTabPresentation(session);
    });
    connect(session,
            &TerminalSessionWidget::sessionStateChanged,
            this,
            [this, session](TerminalSessionWidget::State) { updateTabPresentation(session); });

    const QString title = makeSessionTitle(connection);
    const int index = addTab(session, title);
    setCurrentIndex(index);

    session->start(connection, secret);
    updateTabPresentation(session);
    emit sessionOpened(title);
}

void SessionTabWidget::disconnectCurrentSession()
{
    if (auto *session = activeTerminal()) {
        session->disconnectSession();
    }
}

void SessionTabWidget::reconnectCurrentSession()
{
    if (auto *session = activeTerminal()) {
        session->reconnect();
    }
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
    for (TerminalSessionWidget *session : allTerminals()) {
        session->applySettings();
    }
}

TerminalSessionWidget *SessionTabWidget::activeTerminal() const
{
    return terminalAt(currentIndex());
}

QList<TerminalSessionWidget *> SessionTabWidget::allTerminals() const
{
    QList<TerminalSessionWidget *> sessions;
    for (int i = 0; i < count(); ++i) {
        if (auto *session = terminalAt(i)) {
            sessions.append(session);
        }
    }
    return sessions;
}

void SessionTabWidget::onTabCloseRequested(int index)
{
    if (isWelcomeTab(index)) {
        return;
    }

    const QString name = tabText(index);
    QWidget *page = widget(index);
    removeTab(index);
    if (page) {
        page->deleteLater();
    }

    emit sessionClosed(name);

    if (count() == 0) {
        ensureWelcomeTab();
        refreshWelcome();
    }
}

void SessionTabWidget::onCurrentChanged(int index)
{
    if (index < 0) {
        emit activeSessionChanged(QString());
        return;
    }
    emit activeSessionChanged(tabText(index));
}

void SessionTabWidget::onTabContextMenu(const QPoint &pos)
{
    const int index = tabBar()->tabAt(pos);
    if (index < 0 || isWelcomeTab(index)) {
        return;
    }

    auto *session = terminalAt(index);
    if (!session) {
        return;
    }

    setCurrentIndex(index);

    QMenu menu(this);
    QAction *disconnectAction = menu.addAction(tr("Disconnect"));
    QAction *reconnectAction = menu.addAction(tr("Reconnect"));
    menu.addSeparator();
    QAction *closeAction = menu.addAction(tr("Close"));

    const auto state = session->sessionState();
    disconnectAction->setEnabled(state == TerminalSessionWidget::State::Connected ||
                                 state == TerminalSessionWidget::State::Connecting);
    reconnectAction->setEnabled(state != TerminalSessionWidget::State::Connecting);

    QAction *chosen = menu.exec(tabBar()->mapToGlobal(pos));
    if (chosen == disconnectAction) {
        session->disconnectSession();
    } else if (chosen == reconnectAction) {
        session->reconnect();
    } else if (chosen == closeAction) {
        onTabCloseRequested(index);
    }
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
    connect(welcome, &WelcomeWidget::statusMessage, this, &SessionTabWidget::statusMessage);

    m_welcomeIndex = addTab(welcome, tr("Welcome"));
    tabBar()->setTabButton(m_welcomeIndex, QTabBar::LeftSide, nullptr);
    tabBar()->setTabButton(m_welcomeIndex, QTabBar::RightSide, nullptr);
    setCurrentIndex(m_welcomeIndex);
}

void SessionTabWidget::removeWelcomeTabIfPresent()
{
    if (m_welcomeIndex < 0) {
        return;
    }

    QWidget *page = widget(m_welcomeIndex);
    removeTab(m_welcomeIndex);
    delete page;
    m_welcomeIndex = -1;
}

bool SessionTabWidget::isWelcomeTab(int index) const
{
    return index >= 0 && index == m_welcomeIndex;
}

WelcomeWidget *SessionTabWidget::welcomeWidget() const
{
    if (m_welcomeIndex < 0) {
        return nullptr;
    }
    return qobject_cast<WelcomeWidget *>(widget(m_welcomeIndex));
}

QString SessionTabWidget::makeSessionTitle(const Connection &connection)
{
    const int serial = ++m_sessionSerialByConnection[connection.id];
    if (serial <= 1) {
        return connection.name;
    }
    return QStringLiteral("%1 (#%2)").arg(connection.name).arg(serial);
}

void SessionTabWidget::updateTabPresentation(TerminalSessionWidget *session)
{
    if (!session) {
        return;
    }

    const int index = indexOf(session);
    if (index < 0) {
        return;
    }

    QString tip = session->displayName();
    switch (session->sessionState()) {
    case TerminalSessionWidget::State::Connecting:
        tip += tr(" — Connecting");
        break;
    case TerminalSessionWidget::State::Connected:
        tip += tr(" — Connected");
        break;
    case TerminalSessionWidget::State::Disconnected:
        tip += tr(" — Disconnected");
        break;
    case TerminalSessionWidget::State::Failed:
        tip += tr(" — Failed");
        break;
    }
    setTabToolTip(index, tip);

    if (index == currentIndex()) {
        emit activeSessionChanged(tabText(index));
    }
}

TerminalSessionWidget *SessionTabWidget::terminalAt(int index) const
{
    if (index < 0 || isWelcomeTab(index)) {
        return nullptr;
    }
    return qobject_cast<TerminalSessionWidget *>(widget(index));
}
