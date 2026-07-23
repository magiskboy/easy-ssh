#include "MainWindow.h"

#include "AppSettings.h"
#include "Connection.h"
#include "ConnectionModel.h"
#include "ErrorNotifier.h"
#include "SecretStore.h"
#include "widgets/AboutDialog.h"
#include "widgets/ConnectionListWidget.h"
#include "widgets/FileExplorerWidget.h"
#include "widgets/SessionTabWidget.h"
#include "widgets/SettingsDialog.h"
#include "widgets/ShortcutsDialog.h"
#include "widgets/TerminalSessionWidget.h"
#include "widgets/TunnelListWidget.h"

#include <QAction>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_connectionModel = new ConnectionModel(this);
    m_secretStore = new SecretStore(this);
    m_connectionModel->loadAll();

    setupUi();
    setupMenus();

    ErrorNotifier::setStatusSink([this](const QString &text) {
        setStatusText(text);
    });

    connect(&AppSettings::instance(), &AppSettings::settingsChanged,
            this, &MainWindow::applyAppSettings);
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Easy SSH"));
    resize(1100, 700);

    m_connectionList = new ConnectionListWidget(this);
    m_connectionList->setConnectionModel(m_connectionModel);
    m_connectionList->setSecretStore(m_secretStore);

    m_fileExplorer = new FileExplorerWidget(this);
    m_tunnelList = new TunnelListWidget(this);

    m_sideTabs = new QTabWidget(this);
    m_sideTabs->setDocumentMode(true);
    m_sideTabs->setMinimumWidth(220);
    m_sideTabs->setMaximumWidth(420);
    m_sideTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_sideTabs->addTab(m_connectionList, tr("Connections"));
    m_sideTabs->addTab(m_fileExplorer, tr("Files"));
    m_sideTabs->addTab(m_tunnelList, tr("Tunnels"));

    m_sessionTabs = new SessionTabWidget(this);
    m_sessionTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *rootSplitter = new QSplitter(Qt::Horizontal, this);
    rootSplitter->setChildrenCollapsible(false);
    rootSplitter->addWidget(m_sideTabs);
    rootSplitter->addWidget(m_sessionTabs);
    rootSplitter->setStretchFactor(0, 0);
    rootSplitter->setStretchFactor(1, 1);
    rootSplitter->setSizes({280, 820});

    setCentralWidget(rootSplitter);

    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_statusLabel, 1);

    connect(m_connectionList, &ConnectionListWidget::connectionActivated,
            this, [this](const QUuid &id) {
                const auto connection = m_connectionModel->connectionById(id);
                if (!connection) {
                    return;
                }

                const auto kind = connection->authType == AuthType::PrivateKey
                    ? SecretStore::Kind::Passphrase
                    : SecretStore::Kind::Password;

                setStatusText(tr("Reading credentials for %1…").arg(connection->name));
                m_secretStore->readSecret(id, kind);
            });

    connect(m_secretStore, &SecretStore::readFinished,
            this, [this](const QUuid &connectionId, SecretStore::Kind kind, bool ok,
                         const QString &value, const QString &error) {
                Q_UNUSED(kind);

                const auto connection = m_connectionModel->connectionById(connectionId);
                if (!connection) {
                    return;
                }

                if (!ok) {
                    ErrorNotifier::notify(
                        this,
                        tr("Credentials"),
                        tr("Failed to read credentials: %1").arg(error),
                        ErrorNotifier::Level::Warning);
                    return;
                }

                if (connection->authType == AuthType::Password && value.isEmpty()) {
                    ErrorNotifier::notify(
                        this,
                        tr("Credentials"),
                        tr("No password stored for connection \"%1\". "
                           "Edit the connection and set a password.")
                            .arg(connection->name),
                        ErrorNotifier::Level::Warning);
                    return;
                }

                m_sessionTabs->openSshSession(*connection, value);
            });

    connect(m_connectionList, &ConnectionListWidget::connectionSelected,
            this, [this](const QUuid &id) {
                const auto connection = m_connectionModel->connectionById(id);
                if (!connection) {
                    return;
                }
                setStatusText(tr("Selected: %1").arg(connection->name));
            });

    connect(m_connectionList, &ConnectionListWidget::statusMessage,
            this, &MainWindow::setStatusText);

    connect(m_sessionTabs, &SessionTabWidget::statusMessage,
            this, &MainWindow::setStatusText);

    connect(m_fileExplorer, &FileExplorerWidget::statusMessage,
            this, &MainWindow::setStatusText);

    connect(m_tunnelList, &TunnelListWidget::statusMessage,
            this, &MainWindow::setStatusText);

    connect(m_sessionTabs, &SessionTabWidget::sessionClosed,
            this, [this](const QString &name) {
                setStatusText(tr("Closed session: %1").arg(name));
                updateTerminalActionsEnabled();
                syncFileExplorerToActiveSession();
                syncTunnelsToActiveSession();
            });

    connect(m_sessionTabs, &SessionTabWidget::sessionOpened,
            this, [this](const QString &) {
                updateTerminalActionsEnabled();
            });

    connect(m_sessionTabs, &SessionTabWidget::activeSessionChanged,
            this, [this](const QString &name) {
                updateTerminalActionsEnabled();
                syncFileExplorerToActiveSession();
                syncTunnelsToActiveSession();

                if (name.isEmpty() || name == tr("Welcome")) {
                    setStatusText(tr("Ready"));
                    return;
                }

                if (auto *session = m_sessionTabs->activeTerminal()) {
                    switch (session->sessionState()) {
                    case TerminalSessionWidget::State::Connecting:
                        setStatusText(tr("Connecting: %1").arg(name));
                        return;
                    case TerminalSessionWidget::State::Connected:
                        setStatusText(tr("Connected: %1").arg(name));
                        return;
                    case TerminalSessionWidget::State::Disconnected:
                        setStatusText(tr("Disconnected: %1").arg(name));
                        return;
                    case TerminalSessionWidget::State::Failed:
                        setStatusText(tr("Failed: %1").arg(name));
                        return;
                    }
                }

                setStatusText(tr("Active session: %1").arg(name));
            });
}

QAction *MainWindow::registerAction(const QString &actionId, QAction *action)
{
    action->setShortcut(AppSettings::instance().shortcut(actionId));
    action->setShortcutContext(Qt::ApplicationShortcut);
    m_shortcutActions.insert(actionId, action);
    return action;
}

void MainWindow::setupMenus()
{
    auto *connectionMenu = menuBar()->addMenu(tr("&Connection"));

    auto *newAction = connectionMenu->addAction(tr("&New Connection…"));
    registerAction(QStringLiteral("general.newConnection"), newAction);
    connect(newAction, &QAction::triggered,
            m_connectionList, &ConnectionListWidget::createConnection);

    auto *editAction = connectionMenu->addAction(tr("&Edit…"));
    connect(editAction, &QAction::triggered,
            m_connectionList, &ConnectionListWidget::editSelectedConnection);

    auto *duplicateAction = connectionMenu->addAction(tr("&Duplicate"));
    connect(duplicateAction, &QAction::triggered,
            m_connectionList, &ConnectionListWidget::duplicateSelectedConnection);

    connectionMenu->addSeparator();

    auto *deleteAction = connectionMenu->addAction(tr("&Delete"));
    connect(deleteAction, &QAction::triggered,
            m_connectionList, &ConnectionListWidget::deleteSelectedConnection);

    auto *searchAction = connectionMenu->addAction(tr("&Search"));
    registerAction(QStringLiteral("general.searchConnection"), searchAction);
    connect(searchAction, &QAction::triggered, this, [this]() {
        if (m_sideTabs && m_connectionList) {
            m_sideTabs->setCurrentWidget(m_connectionList);
            m_connectionList->focusSearch();
        }
    });

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));

    auto *settingsAction = editMenu->addAction(tr("&Settings…"));
    registerAction(QStringLiteral("general.settings"), settingsAction);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    auto *shortcutsAction = editMenu->addAction(tr("Keyboard &Shortcuts…"));
    registerAction(QStringLiteral("general.shortcuts"), shortcutsAction);
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::openShortcuts);

    auto *sessionMenu = menuBar()->addMenu(tr("&Session"));

    auto *newSessionAction = sessionMenu->addAction(tr("&New Session"));
    registerAction(QStringLiteral("session.newSession"), newSessionAction);
    connect(newSessionAction, &QAction::triggered,
            m_connectionList, &ConnectionListWidget::openSelectedConnection);

    auto *disconnectAction = sessionMenu->addAction(tr("&Disconnect"));
    connect(disconnectAction, &QAction::triggered,
            m_sessionTabs, &SessionTabWidget::disconnectCurrentSession);

    auto *reconnectAction = sessionMenu->addAction(tr("&Reconnect"));
    registerAction(QStringLiteral("session.reconnect"), reconnectAction);
    connect(reconnectAction, &QAction::triggered,
            m_sessionTabs, &SessionTabWidget::reconnectCurrentSession);

    sessionMenu->addSeparator();

    auto *closeSessionAction = sessionMenu->addAction(tr("&Close"));
    registerAction(QStringLiteral("session.closeSession"), closeSessionAction);
    connect(closeSessionAction, &QAction::triggered,
            m_sessionTabs, &SessionTabWidget::closeCurrentSession);

    auto *nextTabAction = sessionMenu->addAction(tr("&Next Tab"));
    registerAction(QStringLiteral("session.nextTab"), nextTabAction);
    connect(nextTabAction, &QAction::triggered,
            m_sessionTabs, &SessionTabWidget::nextSession);

    auto *prevTabAction = sessionMenu->addAction(tr("&Previous Tab"));
    registerAction(QStringLiteral("session.previousTab"), prevTabAction);
    connect(prevTabAction, &QAction::triggered,
            m_sessionTabs, &SessionTabWidget::previousSession);

    sessionMenu->addSeparator();

    auto *tunnelsAction = sessionMenu->addAction(tr("&Tunnels"));
    connect(tunnelsAction, &QAction::triggered, this, [this]() {
        if (m_sideTabs && m_tunnelList) {
            m_sideTabs->setCurrentWidget(m_tunnelList);
        }
    });

    auto *terminalMenu = menuBar()->addMenu(tr("&Terminal"));

    auto addTerminalAction = [this, terminalMenu](const QString &text,
                                                  const QString &actionId,
                                                  auto method) {
        auto *action = terminalMenu->addAction(text);
        registerAction(actionId, action);
        connect(action, &QAction::triggered, this, [this, method]() {
            if (auto *session = m_sessionTabs->activeTerminal()) {
                (session->*method)();
            }
        });
        m_terminalActions.append(action);
        return action;
    };

    addTerminalAction(tr("&Copy"), QStringLiteral("terminal.copy"),
                      &TerminalSessionWidget::copySelection);
    addTerminalAction(tr("&Paste"), QStringLiteral("terminal.paste"),
                      &TerminalSessionWidget::pasteClipboard);
    terminalMenu->addSeparator();
    addTerminalAction(tr("&Clear Screen"), QStringLiteral("terminal.clearScreen"),
                      &TerminalSessionWidget::clearScreen);
    addTerminalAction(tr("&Search…"), QStringLiteral("terminal.search"),
                      &TerminalSessionWidget::toggleSearch);
    terminalMenu->addSeparator();
    addTerminalAction(tr("Save &Log…"), QStringLiteral("terminal.saveLog"),
                      &TerminalSessionWidget::saveLog);
    addTerminalAction(tr("Save Screensho&t…"), QStringLiteral("terminal.saveScreenshot"),
                      &TerminalSessionWidget::saveScreenshot);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *aboutAction = helpMenu->addAction(tr("&About"));
    registerAction(QStringLiteral("general.about"), aboutAction);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::openAbout);

    updateTerminalActionsEnabled();
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::openShortcuts()
{
    ShortcutsDialog dialog(this);
    dialog.exec();
}

void MainWindow::openAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::applyAppSettings()
{
    if (m_sessionTabs) {
        m_sessionTabs->applySettingsToAllSessions();
    }
    if (m_fileExplorer) {
        m_fileExplorer->applySettings();
        m_fileExplorer->rebindShortcuts();
    }
    rebindShortcuts();
}

void MainWindow::rebindShortcuts()
{
    auto &settings = AppSettings::instance();
    for (auto it = m_shortcutActions.begin(); it != m_shortcutActions.end(); ++it) {
        it.value()->setShortcut(settings.shortcut(it.key()));
    }
}

void MainWindow::setStatusText(const QString &text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}

void MainWindow::updateTerminalActionsEnabled()
{
    const bool enabled = m_sessionTabs && m_sessionTabs->activeTerminal() != nullptr;
    for (QAction *action : m_terminalActions) {
        action->setEnabled(enabled);
    }
}

void MainWindow::syncFileExplorerToActiveSession()
{
    if (!m_fileExplorer || !m_sessionTabs) {
        return;
    }

    auto *session = m_sessionTabs->activeTerminal();
    if (session && session->sessionState() == TerminalSessionWidget::State::Connected) {
        m_fileExplorer->bindSession(session);
        if (m_sideTabs) {
            m_sideTabs->setCurrentWidget(m_fileExplorer);
        }
    } else {
        m_fileExplorer->unbindSession();
    }

    wireActiveSessionStateSync(session);
}

void MainWindow::syncTunnelsToActiveSession()
{
    if (!m_tunnelList || !m_sessionTabs) {
        return;
    }

    auto *session = m_sessionTabs->activeTerminal();
    if (session) {
        m_tunnelList->bindSession(session);
    } else {
        m_tunnelList->unbindSession();
    }

    wireActiveSessionStateSync(session);
}

void MainWindow::wireActiveSessionStateSync(TerminalSessionWidget *session)
{
    if (m_wiredSessionState == session) {
        return;
    }

    if (m_wiredSessionState) {
        disconnect(m_wiredSessionState, &TerminalSessionWidget::sessionStateChanged,
                   this, nullptr);
    }
    m_wiredSessionState = session;
    if (!m_wiredSessionState) {
        return;
    }

    connect(m_wiredSessionState, &TerminalSessionWidget::sessionStateChanged,
            this, [this](TerminalSessionWidget::State) {
                syncFileExplorerToActiveSession();
                syncTunnelsToActiveSession();
            });
}
