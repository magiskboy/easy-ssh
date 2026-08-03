// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "MainWindow.h"

#include "ErrorNotifier.h"
#include "core/connection/Connection.h"
#include "core/connection/ConnectionQuery.h"
#include "core/connection/SecretStore.h"
#include "core/session/Session.h"
#include "core/session/SessionManager.h"
#include "core/settings/AppSettings.h"
#include "core/util/Logging.h"
#include "gui/connection/ConnectionDialog.h"
#include "gui/connection/ConnectionListWidget.h"
#include "gui/connection/ConnectionManagerDialog.h"
#include "gui/connection/ConnectionSecretHelper.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/dialogs/CommandPaletteDialog.h"
#include "gui/dialogs/SettingsDialog.h"
#include "gui/explorer/FileExplorerWidget.h"
#include "gui/models/ConnectionModel.h"
#include "gui/session/SessionPage.h"
#include "gui/session/SessionSideBar.h"
#include "gui/session/SessionTabWidget.h"
#include "gui/tunnel/TunnelListWidget.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QScreen>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_connectionModel = new ConnectionModel(this);
    m_secretStore = new SecretStore(this);
    m_sessionManager = new SessionManager(this);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    AppSettings::instance().setWindowGeometry(saveGeometry());
    QMainWindow::closeEvent(event);
}

void MainWindow::restoreOrDefaultGeometry()
{
    const QByteArray geometry = AppSettings::instance().windowGeometry();
    if (!geometry.isEmpty() && restoreGeometry(geometry)) {
        qCDebug(lcGui) << "restored window geometry" << size();
        return;
    }

    const QSize startup = defaultStartupSize();
    resize(startup);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.x() + (avail.width() - startup.width()) / 2,
             avail.y() + (avail.height() - startup.height()) / 2);
    }
    qCDebug(lcGui) << "default window geometry" << size() << "pos" << pos();
}

QSize MainWindow::defaultStartupSize() const
{
    constexpr QSize kPreferred{1280, 800};
    constexpr QSize kMinimum{1024, 640};

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return kPreferred;
    }

    const QSize avail = screen->availableGeometry().size();
    const QSize scaled(qRound(avail.width() * 0.75), qRound(avail.height() * 0.75));

    // Prefer at least 1280×800 when the screen allows; otherwise fit available area.
    int w = qMax(scaled.width(), kPreferred.width());
    int h = qMax(scaled.height(), kPreferred.height());
    w = qMin(w, avail.width());
    h = qMin(h, avail.height());
    w = qMax(w, qMin(kMinimum.width(), avail.width()));
    h = qMax(h, qMin(kMinimum.height(), avail.height()));
    return {w, h};
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Easy SSH"));
    restoreOrDefaultGeometry();

    // Explicit painted separators — palette(mid) + splitter stylesheets are often invisible
    // under native Linux styles.
    const QString sepColor = palette().color(QPalette::Dark).name();

    m_connectionList = new ConnectionListWidget(this);
    m_connectionList->setConnectionModel(m_connectionModel);
    m_connectionList->setSecretStore(m_secretStore);
    m_connectionList->hide();

    m_sideBar = new SessionSideBar(this);
    m_fileExplorer = new FileExplorerWidget(m_sideBar->fileContainer());
    m_tunnelList = new TunnelListWidget(m_sideBar->tunnelContainer());
    m_tunnelList->setSecretStore(m_secretStore);
    m_sideBar->fileContainer()->layout()->addWidget(m_fileExplorer);
    m_sideBar->tunnelContainer()->layout()->addWidget(m_tunnelList);

    m_sessionTabs = new SessionTabWidget(this);
    m_sessionTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_sessionTabs->setConnectionModel(m_connectionModel);
    m_sessionTabs->setSessionManager(m_sessionManager);

    // Left panel + permanent 1px vertical rule (moves with the panel when resizing).
    auto *sidePanel = new QWidget(this);
    sidePanel->setMinimumWidth(AppSettings::kSidebarMinWidth);
    sidePanel->setMaximumWidth(AppSettings::kSidebarMaxWidth);
    auto *sideLayout = new QHBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);
    sideLayout->addWidget(m_sideBar, 1);

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
    const int sidebarWidth = AppSettings::instance().sidebarWidth();
    const int terminalWidth = qMax(1, width() - sidebarWidth);
    qCDebug(lcGui) << "setupUi splitter sidebarWidth(setting)=" << sidebarWidth
                   << "sidePanel min/max=" << sidePanel->minimumWidth() << sidePanel->maximumWidth()
                   << "windowSize=" << size();
    rootSplitter->setSizes({sidebarWidth, terminalWidth});
    qCDebug(lcGui) << "setupUi splitter sizes after setSizes" << rootSplitter->sizes();
    connect(rootSplitter, &QSplitter::splitterMoved, this, [rootSplitter]() {
        const QList<int> sizes = rootSplitter->sizes();
        qCDebug(lcGui) << "splitterMoved sizes" << sizes;
        AppSettings::instance().setSidebarWidth(sizes.value(0));
    });

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

    connect(m_sessionTabs,
            &SessionTabWidget::openConnectionRequested,
            this,
            &MainWindow::openConnectionById);
    connect(m_sessionTabs,
            &SessionTabWidget::createConnectionRequested,
            m_connectionList,
            &ConnectionListWidget::createConnection);
    connect(m_sessionTabs, &SessionTabWidget::showConnectionsRequested, this, [this]() {
        rebuildConnectionsListMenu();
        if (m_connectionsListMenu) {
            m_connectionsListMenu->popup(QCursor::pos());
        }
    });
    connect(m_sessionTabs,
            &SessionTabWidget::editConnectionRequested,
            this,
            &MainWindow::editConnection);
    connect(m_sessionTabs,
            &SessionTabWidget::deleteConnectionRequested,
            this,
            &MainWindow::deleteConnection);

    connect(m_secretStore,
            &SecretStore::readFinished,
            this,
            [this](const QUuid &connectionId,
                   SecretStore::Kind kind,
                   const QString &value,
                   bool ok,
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
        m_connectionList, &ConnectionListWidget::statusMessage, this, &MainWindow::setStatusText);
    connect(m_connectionList,
            &ConnectionListWidget::connectionEdited,
            this,
            &MainWindow::onConnectionEdited);
    connect(m_connectionList,
            &ConnectionListWidget::manageConnectionsRequested,
            this,
            &MainWindow::openConnectionManager);
    connect(m_connectionList,
            &ConnectionListWidget::statusMessage,
            this,
            [this](const QString &, ErrorNotifier::Level) {
                if (m_sessionTabs) {
                    m_sessionTabs->refreshWelcome();
                }
                rebuildConnectionsListMenu();
            });
    connect(m_connectionModel,
            &QAbstractItemModel::modelReset,
            this,
            &MainWindow::rebuildConnectionsListMenu);
    connect(m_connectionModel,
            &QAbstractItemModel::rowsInserted,
            this,
            &MainWindow::rebuildConnectionsListMenu);
    connect(m_connectionModel,
            &QAbstractItemModel::rowsRemoved,
            this,
            &MainWindow::rebuildConnectionsListMenu);
    connect(m_connectionModel,
            &QAbstractItemModel::dataChanged,
            this,
            &MainWindow::rebuildConnectionsListMenu);

    connect(m_sessionTabs, &SessionTabWidget::statusMessage, this, &MainWindow::setStatusText);

    connect(m_fileExplorer, &FileExplorerWidget::statusMessage, this, &MainWindow::setStatusText);

    connect(m_tunnelList, &TunnelListWidget::statusMessage, this, &MainWindow::setStatusText);

    connect(m_sessionTabs, &SessionTabWidget::sessionClosed, this, [this](const QString &name) {
        setStatusText(tr("Closed session: %1").arg(name), ErrorNotifier::Level::Warning);
        updateTerminalActionsEnabled();
        syncSidePanelsToActiveSession();
        updateSessionStatusInfo();
    });

    connect(m_sessionTabs, &SessionTabWidget::sessionOpened, this, [this](const QString &) {
        updateTerminalActionsEnabled();
        updateSessionStatusInfo();
        syncSidePanelsToActiveSession();
    });

    connect(
        m_sessionTabs, &SessionTabWidget::activeSessionChanged, this, [this](const QString &name) {
            updateTerminalActionsEnabled();
            syncSidePanelsToActiveSession();
            updateSessionStatusInfo();

            if (name.isEmpty() || name == tr("Welcome")) {
                setStatusText(tr("Ready"), ErrorNotifier::Level::Status);
                return;
            }

            if (Session *session = m_sessionTabs->activeSession()) {
                switch (session->state()) {
                case SessionState::Connecting:
                    setStatusText(tr("Connecting: %1").arg(name), ErrorNotifier::Level::Status);
                    return;
                case SessionState::Connected:
                    setStatusText(tr("Connected: %1").arg(name), ErrorNotifier::Level::Success);
                    return;
                case SessionState::Disconnected:
                    setStatusText(tr("Disconnected: %1").arg(name), ErrorNotifier::Level::Warning);
                    return;
                case SessionState::Failed:
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
    auto *connectionsMenu = menuBar()->addMenu(tr("&Connections"));

    auto *newAction = connectionsMenu->addAction(tr("&New…"));
    registerAction(QStringLiteral("general.newConnection"), newAction);
    connect(
        newAction, &QAction::triggered, m_connectionList, &ConnectionListWidget::createConnection);

    auto *quickConnectAction = connectionsMenu->addAction(tr("&Quick Connect…"));
    registerAction(QStringLiteral("general.quickConnect"), quickConnectAction);
    addAction(quickConnectAction);
    connect(quickConnectAction, &QAction::triggered, this, &MainWindow::openQuickConnect);

    auto *managerAction = connectionsMenu->addAction(tr("Connection &Manager…"));
    registerAction(QStringLiteral("general.connectionManager"), managerAction);
    connect(managerAction, &QAction::triggered, this, [this]() { openConnectionManager(); });

    m_connectionsListMenu = connectionsMenu->addMenu(tr("&List"));
    rebuildConnectionsListMenu();

    auto *shellMenu = menuBar()->addMenu(tr("&Shell"));

    auto addTerminalShortcut = [this](const QString &text, auto method, const QString &actionId) {
        auto *action = new QAction(text, this);
        registerAction(actionId, action);
        addAction(action); // Required for shortcuts when the action is not in a menu.
        connect(action, &QAction::triggered, this, [this, method]() {
            if (auto *page = m_sessionTabs->activeSessionPage()) {
                (page->*method)();
            }
        });
        m_terminalActions.append(action);
        return action;
    };

    auto *copyAction = shellMenu->addAction(tr("&Copy"));
    registerAction(QStringLiteral("terminal.copy"), copyAction);
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (auto *page = m_sessionTabs->activeSessionPage()) {
            page->copySelection();
        }
    });
    m_terminalActions.append(copyAction);

    auto *pasteAction = shellMenu->addAction(tr("&Paste"));
    registerAction(QStringLiteral("terminal.paste"), pasteAction);
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (auto *page = m_sessionTabs->activeSessionPage()) {
            page->pasteClipboard();
        }
    });
    m_terminalActions.append(pasteAction);

    addTerminalShortcut(
        tr("&Clear Screen"), &SessionPage::clearScreen, QStringLiteral("terminal.clearScreen"));
    addTerminalShortcut(
        tr("&Search…"), &SessionPage::toggleSearch, QStringLiteral("terminal.search"));
    addTerminalShortcut(
        tr("Save &Log…"), &SessionPage::saveLog, QStringLiteral("terminal.saveLog"));
    addTerminalShortcut(tr("Save Screensho&t…"),
                        &SessionPage::saveScreenshot,
                        QStringLiteral("terminal.saveScreenshot"));

    auto *newShellAction = new QAction(tr("&New Shell"), this);
    registerAction(QStringLiteral("session.newSession"), newShellAction);
    addAction(newShellAction);
    connect(newShellAction, &QAction::triggered, this, [this]() {
        if (Session *session = m_sessionTabs->activeSession()) {
            session->newShell();
        }
    });
    m_terminalActions.append(newShellAction);

    auto *closeShellAction = new QAction(tr("Close &Shell"), this);
    registerAction(QStringLiteral("shell.close"), closeShellAction);
    addAction(closeShellAction);
    connect(closeShellAction, &QAction::triggered, this, [this]() {
        if (Session *session = m_sessionTabs->activeSession()) {
            const QUuid id = session->activeShellId();
            if (!id.isNull()) {
                session->closeShell(id);
            }
        }
    });
    m_terminalActions.append(closeShellAction);

    auto *windowsMenu = menuBar()->addMenu(tr("&Windows"));

    auto *paletteAction = windowsMenu->addAction(tr("Command &Palette…"));
    registerAction(QStringLiteral("general.commandPalette"), paletteAction);
    addAction(paletteAction);
    connect(paletteAction, &QAction::triggered, this, &MainWindow::openCommandPalette);

    auto *goToShellAction = windowsMenu->addAction(tr("&Go to Shell…"));
    registerAction(QStringLiteral("session.goToShell"), goToShellAction);
    addAction(goToShellAction);
    connect(goToShellAction, &QAction::triggered, this, &MainWindow::openGoToShell);

    windowsMenu->addSeparator();

    auto *settingsAction = windowsMenu->addAction(tr("&Settings…"));
    registerAction(QStringLiteral("general.settings"), settingsAction);
    connect(settingsAction, &QAction::triggered, this, [this]() { openSettings(); });

    auto *shortcutsAction = windowsMenu->addAction(tr("Keyboard &Shortcuts…"));
    registerAction(QStringLiteral("general.shortcuts"), shortcutsAction);
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::openShortcuts);

    auto *logAction = windowsMenu->addAction(tr("&Log"));
    connect(logAction, &QAction::triggered, this, &MainWindow::openLogFile);

    windowsMenu->addSeparator();

    auto *nextTabAction = windowsMenu->addAction(tr("&Next Tab"));
    registerAction(QStringLiteral("session.nextTab"), nextTabAction);
    connect(nextTabAction, &QAction::triggered, m_sessionTabs, &SessionTabWidget::nextSession);

    auto *prevTabAction = windowsMenu->addAction(tr("&Previous Tab"));
    registerAction(QStringLiteral("session.previousTab"), prevTabAction);
    connect(prevTabAction, &QAction::triggered, m_sessionTabs, &SessionTabWidget::previousSession);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *aboutAction = helpMenu->addAction(tr("&About"));
    registerAction(QStringLiteral("general.about"), aboutAction);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::openAbout);

    updateTerminalActionsEnabled();
}

void MainWindow::rebuildConnectionsListMenu()
{
    if (!m_connectionsListMenu || !m_connectionModel) {
        return;
    }
    m_connectionsListMenu->clear();
    const int rows = m_connectionModel->rowCount();
    if (rows == 0) {
        auto *empty = m_connectionsListMenu->addAction(tr("(No connections)"));
        empty->setEnabled(false);
        return;
    }
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_connectionModel->index(row, 0);
        const QUuid id = index.data(ConnectionModel::IdRole).toUuid();
        const QString text = index.data(Qt::DisplayRole).toString();
        auto *action = m_connectionsListMenu->addAction(text);
        connect(action, &QAction::triggered, this, [this, id]() { openConnectionById(id); });
    }
}

void MainWindow::openSettings(const QString &initialCategoryId)
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this, initialCategoryId);
    } else if (!initialCategoryId.isEmpty()) {
        m_settingsDialog->selectCategory(initialCategoryId);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MainWindow::openConnectionManager(const QUuid &selectId)
{
    if (!m_connectionManager) {
        m_connectionManager = new ConnectionManagerDialog(this);
        m_connectionManager->setConnectionModel(m_connectionModel);
        m_connectionManager->setSecretStore(m_secretStore);
        connect(m_connectionManager,
                &ConnectionManagerDialog::connectionActivated,
                this,
                &MainWindow::openConnectionById);
        connect(m_connectionManager,
                &ConnectionManagerDialog::connectionEdited,
                this,
                &MainWindow::onConnectionEdited);
        connect(m_connectionManager,
                &ConnectionManagerDialog::statusMessage,
                this,
                &MainWindow::setStatusText);
        connect(m_connectionManager, &QObject::destroyed, this, [this]() {
            rebuildConnectionsListMenu();
            if (m_sessionTabs) {
                m_sessionTabs->refreshWelcome();
            }
        });
    }
    if (!selectId.isNull()) {
        m_connectionManager->selectConnection(selectId);
    }
    m_connectionManager->show();
    m_connectionManager->raise();
    m_connectionManager->activateWindow();
}

void MainWindow::openShortcuts()
{
    openSettings(QStringLiteral("shortcuts"));
}

void MainWindow::ensureCommandPalette()
{
    if (m_commandPalette) {
        return;
    }

    m_commandPalette = new CommandPaletteDialog(this);
    connect(m_commandPalette,
            &CommandPaletteDialog::actionChosen,
            this,
            [this](const QString &actionId) {
                if (actionId == QLatin1String("general.commandPalette") ||
                    actionId == QLatin1String("general.quickConnect") ||
                    actionId == QLatin1String("session.goToShell")) {
                    return;
                }
                if (QAction *action = m_shortcutActions.value(actionId)) {
                    if (action->isEnabled()) {
                        action->trigger();
                    }
                }
            });
    connect(m_commandPalette,
            &CommandPaletteDialog::connectionChosen,
            this,
            &MainWindow::openConnectionById);
    connect(m_commandPalette,
            &CommandPaletteDialog::createConnectionChosen,
            this,
            &MainWindow::createConnectionFromQuery);
    connect(m_commandPalette,
            &CommandPaletteDialog::shellChosen,
            this,
            &MainWindow::focusShell);
}

void MainWindow::populatePaletteActions()
{
    if (!m_commandPalette) {
        return;
    }

    QList<CommandPaletteDialog::ActionItem> items;
    items.reserve(m_shortcutActions.size());
    for (const QString &actionId : AppSettings::shortcutActionIds()) {
        QAction *action = m_shortcutActions.value(actionId);
        if (!action) {
            continue;
        }
        if (actionId == QLatin1String("general.commandPalette") ||
            actionId == QLatin1String("general.quickConnect") ||
            actionId == QLatin1String("session.goToShell")) {
            continue;
        }

        CommandPaletteDialog::ActionItem item;
        item.actionId = actionId;
        item.label = AppSettings::shortcutLabel(actionId);
        item.group = AppSettings::shortcutGroup(actionId);
        item.shortcutText =
            AppSettings::instance().shortcut(actionId).toString(QKeySequence::NativeText);
        item.enabled = action->isEnabled();
        items.append(item);
    }
    m_commandPalette->setActionItems(items);
}

void MainWindow::populatePaletteConnections()
{
    if (!m_commandPalette || !m_connectionModel) {
        return;
    }

    const QList<QUuid> recentIds = AppSettings::instance().recentConnectionIds();
    QHash<QUuid, int> recentRank;
    for (int i = 0; i < recentIds.size(); ++i) {
        recentRank.insert(recentIds.at(i), i);
    }

    QList<CommandPaletteDialog::ConnectionItem> items;
    for (const Connection &connection : m_connectionModel->connections()) {
        CommandPaletteDialog::ConnectionItem item;
        item.id = connection.id;
        item.name = connection.name.isEmpty() ? connection.displayText() : connection.name;
        item.subtitle = QStringLiteral("%1@%2:%3")
                            .arg(connection.username, connection.host)
                            .arg(connection.port);
        item.searchFields = {connection.name,
                             connection.host,
                             connection.username,
                             connection.configAlias,
                             QString::number(connection.port),
                             item.subtitle};
        item.recentRank = recentRank.value(connection.id, -1);
        items.append(item);
    }
    m_commandPalette->setConnectionItems(items);
}

void MainWindow::populatePaletteShells()
{
    if (!m_commandPalette || !m_sessionManager) {
        return;
    }

    QList<CommandPaletteDialog::ShellItem> items;
    const QUuid activeConnectionId =
        m_sessionManager->active() ? m_sessionManager->active()->connectionId() : QUuid();
    const QUuid activeShellId =
        m_sessionManager->active() ? m_sessionManager->active()->activeShellId() : QUuid();

    for (Session *session : m_sessionManager->all()) {
        if (!session) {
            continue;
        }
        const Connection connection = session->connection();
        const QString sessionLabel =
            connection.name.isEmpty()
                ? QStringLiteral("%1@%2").arg(connection.username, connection.host)
                : connection.name;
        for (const ShellChannelState &shell : session->shells()) {
            CommandPaletteDialog::ShellItem item;
            item.connectionId = session->connectionId();
            item.shellId = shell.id;
            item.title = shell.title.isEmpty() ? tr("Shell") : shell.title;
            item.subtitle = sessionLabel;
            item.searchFields = {item.title,
                                 sessionLabel,
                                 connection.name,
                                 connection.host,
                                 connection.username};
            item.isActive =
                session->connectionId() == activeConnectionId && shell.id == activeShellId;
            items.append(item);
        }
    }
    m_commandPalette->setShellItems(items);
}

void MainWindow::openCommandPalette()
{
    ensureCommandPalette();
    populatePaletteActions();
    m_commandPalette->openMode(CommandPaletteDialog::Mode::Actions);
}

void MainWindow::openQuickConnect()
{
    ensureCommandPalette();
    populatePaletteConnections();
    m_commandPalette->openMode(CommandPaletteDialog::Mode::Connections);
}

void MainWindow::openGoToShell()
{
    ensureCommandPalette();
    populatePaletteShells();
    m_commandPalette->openMode(CommandPaletteDialog::Mode::Shells);
}

void MainWindow::createConnectionFromQuery(const QString &query)
{
    auto *dialog = new ConnectionDialog(ConnectionDialog::Mode::Create, this);
    if (!query.trimmed().isEmpty()) {
        dialog->setConnection(ConnectionQuery::draftFromQuery(query));
    }
    connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        const Connection connection = dialog->connection();
        if (!m_connectionModel->add(connection)) {
            ErrorNotifier::notify(this,
                                  tr("Error"),
                                  tr("Failed to create connection."),
                                  ErrorNotifier::Level::Warning);
            return;
        }

        ConnectionSecretHelper::persistSecrets(m_secretStore,
                                               connection,
                                               AuthType::Password,
                                               false,
                                               dialog->password(),
                                               dialog->passwordProvided(),
                                               dialog->passphrase(),
                                               dialog->passphraseProvided(),
                                               dialog->gatewayPassword(),
                                               dialog->gatewayPasswordProvided(),
                                               dialog->gatewayPassphrase(),
                                               dialog->gatewayPassphraseProvided());

        rebuildConnectionsListMenu();
        if (m_sessionTabs) {
            m_sessionTabs->refreshWelcome();
        }
        setStatusText(tr("Created connection: %1").arg(connection.name),
                      ErrorNotifier::Level::Success);
        openConnectionById(connection.id);
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::focusShell(const QUuid &connectionId, const QUuid &shellId)
{
    if (!m_sessionTabs || connectionId.isNull() || shellId.isNull()) {
        return;
    }
    if (!m_sessionTabs->activateConnection(connectionId)) {
        return;
    }
    if (Session *session = m_sessionManager->get(connectionId)) {
        session->setActiveShell(shellId);
    }
}

void MainWindow::openAbout()
{
    if (!m_aboutDialog) {
        m_aboutDialog = new AboutDialog(this);
    }
    m_aboutDialog->show();
    m_aboutDialog->raise();
    m_aboutDialog->activateWindow();
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
    const bool enabled = m_sessionTabs && m_sessionTabs->activeSessionPage() != nullptr;
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

    Session *session = m_sessionTabs ? m_sessionTabs->activeSession() : nullptr;
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

    QString shellName = QStringLiteral("—");
    for (const ShellChannelState &shell : session->shells()) {
        if (shell.id == session->activeShellId()) {
            shellName = shell.title;
            break;
        }
    }

    QString ttl = QStringLiteral("—");
    if (session->state() == SessionState::Connected && session->connectedAt().isValid()) {
        const qint64 seconds = session->connectedAt().secsTo(QDateTime::currentDateTimeUtc());
        ttl = formatSessionTtl(qMax<qint64>(0, seconds));
        if (m_sessionInfoTimer && !m_sessionInfoTimer->isActive()) {
            m_sessionInfoTimer->start();
        }
    } else if (m_sessionInfoTimer) {
        m_sessionInfoTimer->stop();
    }

    m_sessionInfoLabel->setText(tr("%1 | %2 | %3 | %4").arg(host, user, shellName, ttl));
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

void MainWindow::syncSidePanelsToActiveSession()
{
    Session *session = m_sessionTabs ? m_sessionTabs->activeSession() : nullptr;
    if (m_sideBar) {
        if (session) {
            m_sideBar->bindSession(session);
        } else {
            m_sideBar->unbindSession();
        }
    }

    if (session && session->state() == SessionState::Connected) {
        m_fileExplorer->bindSession(session);
        m_tunnelList->bindSession(session);
    } else if (session) {
        m_fileExplorer->unbindSession();
        m_tunnelList->bindSession(session);
    } else {
        m_fileExplorer->unbindSession();
        m_tunnelList->unbindSession();
    }

    wireActiveSessionStateSync(session);
}

void MainWindow::wireActiveSessionStateSync(Session *session)
{
    if (m_wiredSessionState == session) {
        return;
    }

    if (m_wiredSessionState) {
        disconnect(m_wiredSessionState, &Session::stateChanged, this, nullptr);
    }
    m_wiredSessionState = session;
    if (!m_wiredSessionState) {
        return;
    }

    connect(m_wiredSessionState, &Session::stateChanged, this, [this](SessionState) {
        syncSidePanelsToActiveSession();
        updateSessionStatusInfo();
    });
}

void MainWindow::editConnection(const QUuid &id)
{
    openConnectionManager(id);
}

void MainWindow::onConnectionEdited(const QUuid &id,
                                    bool connectivityChanged,
                                    bool targetSecretUpdated,
                                    const QString &targetSecret,
                                    bool gatewaySecretUpdated,
                                    const QString &gatewaySecret)
{
    if (const auto updated = m_connectionModel->connectionById(id)) {
        if (Session *session = m_sessionManager ? m_sessionManager->get(id) : nullptr) {
            session->setConnection(*updated);
            if (targetSecretUpdated || gatewaySecretUpdated) {
                SessionCredentials creds = session->credentials();
                if (targetSecretUpdated) {
                    creds.targetSecret = targetSecret;
                }
                if (gatewaySecretUpdated) {
                    creds.gatewaySecret = gatewaySecret;
                }
                session->setCredentials(creds);
            }
            // Include Failed/Disconnected: E8 edits the password after a failed login, then
            // needs an immediate retry with the dialog secret (keychain write is async).
            if (connectivityChanged) {
                const auto answer = QMessageBox::question(
                    this,
                    tr("Reconnect?"),
                    tr("Connection settings that affect SSH changed. Reconnect now?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
                if (answer == QMessageBox::Yes) {
                    // Credentials already refreshed from the dialog when secrets changed;
                    // reconnect with in-memory secrets to avoid racing the async keychain write.
                    session->reconnect();
                }
            }
        }
    }
    if (m_sessionTabs) {
        m_sessionTabs->refreshConnectionPresentation(id);
        m_sessionTabs->refreshWelcome();
    }
    rebuildConnectionsListMenu();
    updateSessionStatusInfo();
}

void MainWindow::deleteConnection(const QUuid &id)
{
    if (id.isNull()) {
        return;
    }
    const auto connection = m_connectionModel->connectionById(id);
    if (!connection) {
        return;
    }
    const auto answer = QMessageBox::question(this,
                                              tr("Delete Connection"),
                                              tr("Delete connection \"%1\"?").arg(connection->name),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    for (int i = 0; i < m_sessionTabs->count(); ++i) {
        if (SessionPage *page = qobject_cast<SessionPage *>(m_sessionTabs->widget(i))) {
            if (page->session() && page->session()->connectionId() == id) {
                m_sessionTabs->setCurrentIndex(i);
                m_sessionTabs->closeCurrentSession();
                break;
            }
        }
    }
    if (m_sessionManager && m_sessionManager->get(id) != nullptr) {
        m_sessionManager->close(id);
    }
    m_connectionModel->removeById(id);
    rebuildConnectionsListMenu();
    m_sessionTabs->refreshWelcome();
}
