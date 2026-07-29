#include "MainWindow.h"

#include "AppSettings.h"
#include "Connection.h"
#include "ConnectionModel.h"
#include "ErrorNotifier.h"
#include "Logging.h"
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
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_connectionModel = new ConnectionModel(this);
    m_secretStore = new SecretStore(this);
    m_connectionModel->loadAll();

    setupUi();
    setupMenus();

    ErrorNotifier::setStatusSink(
        [this](const QString &text, ErrorNotifier::Level level) { setStatusText(text, level); });

    connect(&AppSettings::instance(),
            &AppSettings::settingsChanged,
            this,
            &MainWindow::applyAppSettings);
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Easy SSH"));
    resize(1100, 700);

    // Explicit painted separators — palette(mid) + splitter stylesheets are often invisible
    // under native Linux styles.
    const QString sepColor = palette().color(QPalette::Dark).name();

    m_connectionList = new ConnectionListWidget(this);
    m_connectionList->setConnectionModel(m_connectionModel);
    m_connectionList->setSecretStore(m_secretStore);

    m_fileExplorer = new FileExplorerWidget(this);
    m_tunnelList = new TunnelListWidget(this);

    m_sideTabs = new QTabWidget(this);
    m_sideTabs->setDocumentMode(true);
    m_sideTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_sideTabs->addTab(m_connectionList, tr("Connections"));
    m_sideTabs->addTab(m_fileExplorer, tr("Files"));
    m_sideTabs->addTab(m_tunnelList, tr("Tunnels"));

    m_sessionTabs = new SessionTabWidget(this);
    m_sessionTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_sessionTabs->setConnectionModel(m_connectionModel);

    // Left panel + permanent 1px vertical rule (moves with the panel when resizing).
    auto *sidePanel = new QWidget(this);
    sidePanel->setMinimumWidth(220);
    sidePanel->setMaximumWidth(420);
    auto *sideLayout = new QHBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);
    sideLayout->addWidget(m_sideTabs, 1);

    auto *sideSeparator = new QFrame(sidePanel);
    sideSeparator->setFrameShape(QFrame::NoFrame);
    sideSeparator->setFixedWidth(1);
    sideSeparator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    sideSeparator->setStyleSheet(
        QStringLiteral("background-color: %1; border: none;").arg(sepColor));
    sideLayout->addWidget(sideSeparator);

    auto *rootSplitter = new QSplitter(Qt::Horizontal, this);
    rootSplitter->setChildrenCollapsible(false);
    rootSplitter->setHandleWidth(2);
    rootSplitter->addWidget(sidePanel);
    rootSplitter->addWidget(m_sessionTabs);
    rootSplitter->setStretchFactor(0, 0);
    rootSplitter->setStretchFactor(1, 1);
    rootSplitter->setSizes({280, 820});

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(rootSplitter, 1);

    // Horizontal rule above the status bar so the bottom region is clearly separated.
    auto *statusSeparator = new QFrame(central);
    statusSeparator->setFrameShape(QFrame::NoFrame);
    statusSeparator->setFixedHeight(1);
    statusSeparator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    statusSeparator->setStyleSheet(
        QStringLiteral("background-color: %1; border: none;").arg(sepColor));
    rootLayout->addWidget(statusSeparator);

    setCentralWidget(central);

    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_statusLabel, 1);

    m_sessionInfoLabel = new QLabel(this);
    m_sessionInfoLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sessionInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusBar()->addPermanentWidget(m_sessionInfoLabel);

    m_sessionInfoTimer = new QTimer(this);
    m_sessionInfoTimer->setInterval(1000);
    connect(m_sessionInfoTimer, &QTimer::timeout, this, &MainWindow::updateSessionStatusInfo);
    updateSessionStatusInfo();

    connect(m_connectionList,
            &ConnectionListWidget::connectionActivated,
            this,
            &MainWindow::openConnectionById);

    connect(m_sessionTabs,
            &SessionTabWidget::openConnectionRequested,
            this,
            &MainWindow::openConnectionById);
    connect(m_sessionTabs,
            &SessionTabWidget::createConnectionRequested,
            m_connectionList,
            &ConnectionListWidget::createConnection);
    connect(m_sessionTabs, &SessionTabWidget::showConnectionsRequested, this, [this]() {
        if (m_sideTabs && m_connectionList) {
            m_sideTabs->setCurrentWidget(m_connectionList);
            m_connectionList->focusSearch();
        }
    });

    connect(m_secretStore,
            &SecretStore::readFinished,
            this,
            [this](const QUuid &connectionId,
                   SecretStore::Kind kind,
                   bool ok,
                   const QString &value,
                   const QString &error) {
                const auto connection = m_connectionModel->connectionById(connectionId);
                if (!connection) {
                    return;
                }

                if (!ok) {
                    if (kind == SecretStore::Kind::GatewayPassphrase &&
                        connection->usesJumpHost() &&
                        !connection->jumpHops.first().useTargetCredentials &&
                        connection->jumpHops.first().authType == AuthType::PrivateKey) {
                        m_pendingCredentials.gatewaySecret.clear();
                        m_pendingNeedTargetSecret = true;
                        readTargetSecretForConnect(*connection);
                        return;
                    }

                    if (kind == SecretStore::Kind::Passphrase &&
                        connection->authType == AuthType::PrivateKey) {
                        m_pendingCredentials.targetSecret.clear();
                        finishConnect(*connection, m_pendingCredentials);
                        return;
                    }

                    ErrorNotifier::notify(this,
                                          tr("Credentials"),
                                          tr("Failed to read credentials: %1").arg(error),
                                          ErrorNotifier::Level::Warning);
                    m_pendingConnectId = {};
                    m_pendingNeedTargetSecret = false;
                    m_pendingCredentials = {};
                    return;
                }

                if (kind == SecretStore::Kind::GatewayPassword ||
                    kind == SecretStore::Kind::GatewayPassphrase) {
                    m_pendingCredentials.gatewaySecret = value;
                    m_pendingConnectId = connectionId;
                    m_pendingNeedTargetSecret = true;
                    readTargetSecretForConnect(*connection);
                    return;
                }

                m_pendingCredentials.targetSecret = value;

                if (connection->authType == AuthType::Password && value.isEmpty()) {
                    ErrorNotifier::notify(this,
                                          tr("Credentials"),
                                          tr("No password stored for connection \"%1\". "
                                             "Edit the connection and set a password.")
                                              .arg(connection->name),
                                          ErrorNotifier::Level::Warning);
                    m_pendingConnectId = {};
                    m_pendingNeedTargetSecret = false;
                    m_pendingCredentials = {};
                    return;
                }

                finishConnect(*connection, m_pendingCredentials);
            });

    connect(
        m_connectionList, &ConnectionListWidget::connectionSelected, this, [this](const QUuid &id) {
            const auto connection = m_connectionModel->connectionById(id);
            if (!connection) {
                return;
            }
            setStatusText(tr("Selected: %1").arg(connection->name));
        });

    connect(
        m_connectionList, &ConnectionListWidget::statusMessage, this, &MainWindow::setStatusText);
    connect(m_connectionList,
            &ConnectionListWidget::statusMessage,
            this,
            [this](const QString &, ErrorNotifier::Level) {
                if (m_sessionTabs) {
                    m_sessionTabs->refreshWelcome();
                }
            });

    connect(m_sessionTabs, &SessionTabWidget::statusMessage, this, &MainWindow::setStatusText);

    connect(m_fileExplorer, &FileExplorerWidget::statusMessage, this, &MainWindow::setStatusText);

    connect(m_tunnelList, &TunnelListWidget::statusMessage, this, &MainWindow::setStatusText);

    connect(m_sessionTabs, &SessionTabWidget::sessionClosed, this, [this](const QString &name) {
        setStatusText(tr("Closed session: %1").arg(name), ErrorNotifier::Level::Warning);
        updateTerminalActionsEnabled();
        syncFileExplorerToActiveSession();
        syncTunnelsToActiveSession();
        updateSessionStatusInfo();
    });

    connect(m_sessionTabs, &SessionTabWidget::sessionOpened, this, [this](const QString &) {
        updateTerminalActionsEnabled();
        updateSessionStatusInfo();
    });

    connect(
        m_sessionTabs, &SessionTabWidget::activeSessionChanged, this, [this](const QString &name) {
            updateTerminalActionsEnabled();
            syncFileExplorerToActiveSession();
            syncTunnelsToActiveSession();
            updateSessionStatusInfo();

            if (name.isEmpty() || name == tr("Welcome")) {
                setStatusText(tr("Ready"), ErrorNotifier::Level::Status);
                return;
            }

            if (auto *session = m_sessionTabs->activeTerminal()) {
                switch (session->sessionState()) {
                case TerminalSessionWidget::State::Connecting:
                    setStatusText(tr("Connecting: %1").arg(name), ErrorNotifier::Level::Status);
                    return;
                case TerminalSessionWidget::State::Connected:
                    setStatusText(tr("Connected: %1").arg(name), ErrorNotifier::Level::Success);
                    return;
                case TerminalSessionWidget::State::Disconnected:
                    setStatusText(tr("Disconnected: %1").arg(name), ErrorNotifier::Level::Warning);
                    return;
                case TerminalSessionWidget::State::Failed:
                    setStatusText(tr("Failed: %1").arg(name), ErrorNotifier::Level::Error);
                    return;
                }
            }

            setStatusText(tr("Active session: %1").arg(name), ErrorNotifier::Level::Status);
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
    connect(
        newAction, &QAction::triggered, m_connectionList, &ConnectionListWidget::createConnection);

    auto *editAction = connectionMenu->addAction(tr("&Edit…"));
    connect(editAction,
            &QAction::triggered,
            m_connectionList,
            &ConnectionListWidget::editSelectedConnection);

    auto *duplicateAction = connectionMenu->addAction(tr("&Duplicate"));
    connect(duplicateAction,
            &QAction::triggered,
            m_connectionList,
            &ConnectionListWidget::duplicateSelectedConnection);

    auto *importAction = connectionMenu->addAction(tr("&Import from SSH Config…"));
    connect(importAction,
            &QAction::triggered,
            m_connectionList,
            &ConnectionListWidget::importSelectedFromSshConfig);

    connectionMenu->addSeparator();

    auto *deleteAction = connectionMenu->addAction(tr("&Delete"));
    connect(deleteAction,
            &QAction::triggered,
            m_connectionList,
            &ConnectionListWidget::deleteSelectedConnection);

    auto *reloadConfigAction = connectionMenu->addAction(tr("&Reload SSH Config"));
    connect(reloadConfigAction,
            &QAction::triggered,
            m_connectionList,
            &ConnectionListWidget::reloadSshConfig);

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
    connect(newSessionAction,
            &QAction::triggered,
            m_connectionList,
            &ConnectionListWidget::openSelectedConnection);

    auto *disconnectAction = sessionMenu->addAction(tr("&Disconnect"));
    connect(disconnectAction,
            &QAction::triggered,
            m_sessionTabs,
            &SessionTabWidget::disconnectCurrentSession);

    auto *reconnectAction = sessionMenu->addAction(tr("&Reconnect"));
    registerAction(QStringLiteral("session.reconnect"), reconnectAction);
    connect(reconnectAction,
            &QAction::triggered,
            m_sessionTabs,
            &SessionTabWidget::reconnectCurrentSession);

    sessionMenu->addSeparator();

    auto *closeSessionAction = sessionMenu->addAction(tr("&Close"));
    registerAction(QStringLiteral("session.closeSession"), closeSessionAction);
    connect(closeSessionAction,
            &QAction::triggered,
            m_sessionTabs,
            &SessionTabWidget::closeCurrentSession);

    auto *nextTabAction = sessionMenu->addAction(tr("&Next Tab"));
    registerAction(QStringLiteral("session.nextTab"), nextTabAction);
    connect(nextTabAction, &QAction::triggered, m_sessionTabs, &SessionTabWidget::nextSession);

    auto *prevTabAction = sessionMenu->addAction(tr("&Previous Tab"));
    registerAction(QStringLiteral("session.previousTab"), prevTabAction);
    connect(prevTabAction, &QAction::triggered, m_sessionTabs, &SessionTabWidget::previousSession);

    sessionMenu->addSeparator();

    auto *tunnelsAction = sessionMenu->addAction(tr("&Tunnels"));
    connect(tunnelsAction, &QAction::triggered, this, [this]() {
        if (m_sideTabs && m_tunnelList) {
            m_sideTabs->setCurrentWidget(m_tunnelList);
        }
    });

    auto *terminalMenu = menuBar()->addMenu(tr("&Terminal"));

    auto addTerminalAction =
        [this, terminalMenu](const QString &text, const QString &actionId, auto method) {
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

    addTerminalAction(
        tr("&Copy"), QStringLiteral("terminal.copy"), &TerminalSessionWidget::copySelection);
    addTerminalAction(
        tr("&Paste"), QStringLiteral("terminal.paste"), &TerminalSessionWidget::pasteClipboard);
    terminalMenu->addSeparator();
    addTerminalAction(tr("&Clear Screen"),
                      QStringLiteral("terminal.clearScreen"),
                      &TerminalSessionWidget::clearScreen);
    addTerminalAction(
        tr("&Search…"), QStringLiteral("terminal.search"), &TerminalSessionWidget::toggleSearch);
    terminalMenu->addSeparator();
    addTerminalAction(
        tr("Save &Log…"), QStringLiteral("terminal.saveLog"), &TerminalSessionWidget::saveLog);
    addTerminalAction(tr("Save Screensho&t…"),
                      QStringLiteral("terminal.saveScreenshot"),
                      &TerminalSessionWidget::saveScreenshot);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *openLogAction = helpMenu->addAction(tr("Open &Log"));
    connect(openLogAction, &QAction::triggered, this, &MainWindow::openLogFile);
    helpMenu->addSeparator();
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

void MainWindow::openLogFile()
{
    const QString path = logFilePath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        ErrorNotifier::notify(this,
                              tr("Open Log"),
                              tr("Cannot open log file with the system application: %1").arg(path),
                              ErrorNotifier::Level::Error);
        return;
    }
    setStatusText(tr("Opened log file"), ErrorNotifier::Level::Status);
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

void MainWindow::setStatusText(const QString &text, ErrorNotifier::Level level)
{
    if (!m_statusLabel) {
        return;
    }

    m_statusLabel->setText(text);

    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    QString color;
    switch (level) {
    case ErrorNotifier::Level::Status:
        m_statusLabel->setStyleSheet(QString());
        return;
    case ErrorNotifier::Level::Success:
        color = dark ? QStringLiteral("#81c784") : QStringLiteral("#2e7d32");
        break;
    case ErrorNotifier::Level::Warning:
        color = dark ? QStringLiteral("#ffb74d") : QStringLiteral("#ef6c00");
        break;
    case ErrorNotifier::Level::Error:
        color = dark ? QStringLiteral("#ef5350") : QStringLiteral("#c62828");
        break;
    }
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}

void MainWindow::updateTerminalActionsEnabled()
{
    const bool enabled = m_sessionTabs && m_sessionTabs->activeTerminal() != nullptr;
    for (QAction *action : m_terminalActions) {
        action->setEnabled(enabled);
    }
}

void MainWindow::openConnectionById(const QUuid &id)
{
    const auto connection = m_connectionModel->connectionById(id);
    if (!connection) {
        setStatusText(tr("Connection not found."), ErrorNotifier::Level::Warning);
        return;
    }

    m_pendingConnectId = id;
    m_pendingNeedTargetSecret = false;
    m_pendingCredentials = {};

    setStatusText(tr("Reading credentials for %1…").arg(connection->name),
                  ErrorNotifier::Level::Status);

    if (connection->usesJumpHost() && !connection->jumpHops.first().useTargetCredentials) {
        const JumpHop &hop = connection->jumpHops.first();
        const auto gatewayKind = hop.authType == AuthType::PrivateKey
                                     ? SecretStore::Kind::GatewayPassphrase
                                     : SecretStore::Kind::GatewayPassword;
        m_secretStore->readSecret(id, gatewayKind);
        return;
    }

    readTargetSecretForConnect(*connection);
}

void MainWindow::readTargetSecretForConnect(const Connection &connection)
{
    const auto kind = connection.authType == AuthType::PrivateKey ? SecretStore::Kind::Passphrase
                                                                  : SecretStore::Kind::Password;
    m_secretStore->readSecret(connection.id, kind);
}

void MainWindow::finishConnect(const Connection &connection, const SessionCredentials &credentials)
{
    m_sessionTabs->openSshSession(connection, credentials);
    AppSettings::instance().recordRecentConnection(connection.id);
    m_sessionTabs->refreshWelcome();
    m_pendingConnectId = {};
    m_pendingNeedTargetSecret = false;
    m_pendingCredentials = {};
}

void MainWindow::updateSessionStatusInfo()
{
    if (!m_sessionInfoLabel) {
        return;
    }

    auto *session = m_sessionTabs ? m_sessionTabs->activeTerminal() : nullptr;
    if (!session) {
        m_sessionInfoLabel->clear();
        if (m_sessionInfoTimer) {
            m_sessionInfoTimer->stop();
        }
        return;
    }

    const Connection connection = session->connection();
    const QString host = connection.host.isEmpty() ? QStringLiteral("—") : connection.host;
    const QString user = connection.username.isEmpty() ? QStringLiteral("—") : connection.username;

    QString ttl = QStringLiteral("—");
    if (session->sessionState() == TerminalSessionWidget::State::Connected &&
        session->connectedAt().isValid()) {
        const qint64 seconds = session->connectedAt().secsTo(QDateTime::currentDateTimeUtc());
        ttl = formatSessionTtl(qMax<qint64>(0, seconds));
        if (m_sessionInfoTimer && !m_sessionInfoTimer->isActive()) {
            m_sessionInfoTimer->start();
        }
    } else if (m_sessionInfoTimer) {
        m_sessionInfoTimer->stop();
    }

    m_sessionInfoLabel->setText(tr("%1 | %2 | %3").arg(host, user, ttl));
}

QString MainWindow::formatSessionTtl(qint64 seconds)
{
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
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
        disconnect(m_wiredSessionState, &TerminalSessionWidget::sessionStateChanged, this, nullptr);
    }
    m_wiredSessionState = session;
    if (!m_wiredSessionState) {
        return;
    }

    connect(m_wiredSessionState,
            &TerminalSessionWidget::sessionStateChanged,
            this,
            [this](TerminalSessionWidget::State) {
                syncFileExplorerToActiveSession();
                syncTunnelsToActiveSession();
                updateSessionStatusInfo();
            });
}
