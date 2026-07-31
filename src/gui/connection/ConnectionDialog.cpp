// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionDialog.h"

#include "gui/widgets/CategoryDialogShell.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_id(QUuid::createUuid())
{
    setupUi();
    setWindowTitle(mode == Mode::Create ? tr("New Connection") : tr("Edit Connection"));
    resize(640, 520);
}

void ConnectionDialog::setupUi()
{
    m_shell = new CategoryDialogShell(this);
    m_shell->addPage(nullptr, tr("Session"), createSessionPage(), QStringLiteral("session"));
    m_shell->addPage(
        nullptr, tr("Connection"), createConnectionPage(), QStringLiteral("connection"));
    // SSH Proxy (ProxyJump / ProxyCommand) — not port-forward tunnels (those live in the session
    // sidebar).
    m_shell->addPage(nullptr, tr("SSH Proxy"), createProxyPage(), QStringLiteral("ssh-proxy"));

    QTreeWidgetItem *environmentGroup = m_shell->addGroup(tr("Environment"));
    m_shell->addPage(
        environmentGroup, tr("SCP/Shell"), createScpShellPage(), QStringLiteral("scp-shell"));
    m_shell->expandAll();
    m_shell->selectFirst();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_shell, 1);
    layout->addWidget(buttons);

    updateAuthFieldsVisibility();
    updateProxyPanelVisibility();
    updateGatewayAuthFieldsVisibility();
}

QWidget *ConnectionDialog::createSessionPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *targetGroup = new QGroupBox(tr("Target"), page);
    auto *targetForm = new QFormLayout(targetGroup);

    m_nameEdit = new QLineEdit(targetGroup);
    m_hostEdit = new QLineEdit(targetGroup);
    m_portSpin = new QSpinBox(targetGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(22);
    m_usernameEdit = new QLineEdit(targetGroup);
    m_startupDirEdit = new QLineEdit(targetGroup);
    m_startupDirEdit->setPlaceholderText(tr("Optional"));

    targetForm->addRow(tr("Name"), m_nameEdit);
    targetForm->addRow(tr("Host"), m_hostEdit);
    targetForm->addRow(tr("Port"), m_portSpin);
    targetForm->addRow(tr("Username"), m_usernameEdit);
    targetForm->addRow(tr("Startup Directory"), m_startupDirEdit);

    auto *authGroup = new QGroupBox(tr("SSH Authentication"), page);
    m_authForm = new QFormLayout(authGroup);

    m_authTypeCombo = new QComboBox(authGroup);
    m_authTypeCombo->addItem(tr("Password"), static_cast<int>(AuthType::Password));
    m_authTypeCombo->addItem(tr("Private Key"), static_cast<int>(AuthType::PrivateKey));

    m_passwordEdit = new QLineEdit(authGroup);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_passwordEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_privateKeyEdit = new QLineEdit(authGroup);
    auto *browseButton = new QPushButton(tr("Browse…"), authGroup);
    auto *keyLayout = new QHBoxLayout();
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(m_privateKeyEdit, 1);
    keyLayout->addWidget(browseButton);
    m_privateKeyRow = new QWidget(authGroup);
    m_privateKeyRow->setLayout(keyLayout);

    m_passphraseEdit = new QLineEdit(authGroup);
    m_passphraseEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_passphraseEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_authForm->addRow(tr("Authentication"), m_authTypeCombo);
    m_authForm->addRow(tr("Password"), m_passwordEdit);
    m_authForm->addRow(tr("Private Key"), m_privateKeyRow);
    m_authForm->addRow(tr("Passphrase"), m_passphraseEdit);

    layout->addWidget(targetGroup);
    layout->addWidget(authGroup);
    layout->addStretch(1);

    connect(m_authTypeCombo,
            &QComboBox::currentIndexChanged,
            this,
            &ConnectionDialog::onAuthTypeChanged);
    connect(browseButton, &QPushButton::clicked, this, &ConnectionDialog::browsePrivateKey);

    return page;
}

QWidget *ConnectionDialog::createConnectionPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Connection"), page);
    auto *form = new QFormLayout(group);

    m_keepAliveIntervalSpin = new QSpinBox(group);
    m_keepAliveIntervalSpin->setRange(0, 3600);
    m_keepAliveIntervalSpin->setSuffix(tr(" s"));
    m_keepAliveIntervalSpin->setSpecialValueText(tr("Disabled"));
    m_keepAliveIntervalSpin->setValue(0);

    m_keepAliveCountSpin = new QSpinBox(group);
    m_keepAliveCountSpin->setRange(1, 10);
    m_keepAliveCountSpin->setValue(3);
    m_keepAliveCountSpin->setEnabled(false);

    m_compressionCheck = new QCheckBox(tr("Enable SSH compression"), group);

    form->addRow(tr("Keep-alive interval"), m_keepAliveIntervalSpin);
    form->addRow(tr("Keep-alive max retries"), m_keepAliveCountSpin);
    form->addRow(QString(), m_compressionCheck);

    layout->addWidget(group);
    layout->addStretch(1);

    connect(m_keepAliveIntervalSpin, &QSpinBox::valueChanged, this, [this](int value) {
        m_keepAliveCountSpin->setEnabled(value > 0);
    });

    return page;
}

QWidget *ConnectionDialog::createProxyPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *modeGroup = new QGroupBox(tr("Mode"), page);
    auto *modeLayout = new QHBoxLayout(modeGroup);
    m_proxyNoneRadio = new QRadioButton(tr("None"), modeGroup);
    m_proxyJumpRadio = new QRadioButton(tr("ProxyJump"), modeGroup);
    m_proxyCommandRadio = new QRadioButton(tr("ProxyCommand"), modeGroup);
    m_proxyNoneRadio->setChecked(true);

    m_proxyModeGroup = new QButtonGroup(page);
    m_proxyModeGroup->addButton(m_proxyNoneRadio, static_cast<int>(SshProxyMode::None));
    m_proxyModeGroup->addButton(m_proxyJumpRadio, static_cast<int>(SshProxyMode::ProxyJump));
    m_proxyModeGroup->addButton(m_proxyCommandRadio, static_cast<int>(SshProxyMode::ProxyCommand));

    modeLayout->addWidget(m_proxyNoneRadio);
    modeLayout->addWidget(m_proxyJumpRadio);
    modeLayout->addWidget(m_proxyCommandRadio);
    modeLayout->addStretch(1);

    m_jumpPanel = new QGroupBox(tr("ProxyJump"), page);
    auto *gatewayLayout = new QVBoxLayout(m_jumpPanel);

    auto *hint = new QLabel(tr("Route: local → gateway → target"), m_jumpPanel);
    hint->setWordWrap(true);
    gatewayLayout->addWidget(hint);

    auto *hopButtonsLayout = new QHBoxLayout();
    m_hopList = new QListWidget(m_jumpPanel);
    m_hopList->setMaximumHeight(72);
    m_addHopButton = new QPushButton(tr("Add hop"), m_jumpPanel);
    m_removeHopButton = new QPushButton(tr("Remove hop"), m_jumpPanel);
    hopButtonsLayout->addWidget(m_addHopButton);
    hopButtonsLayout->addWidget(m_removeHopButton);
    hopButtonsLayout->addStretch(1);
    gatewayLayout->addWidget(m_hopList);
    gatewayLayout->addLayout(hopButtonsLayout);

    m_gatewayForm = new QFormLayout();
    m_gatewayHostEdit = new QLineEdit(m_jumpPanel);
    m_gatewayPortSpin = new QSpinBox(m_jumpPanel);
    m_gatewayPortSpin->setRange(1, 65535);
    m_gatewayPortSpin->setValue(22);
    m_gatewayUsernameEdit = new QLineEdit(m_jumpPanel);
    m_useTargetCredentialsCheck = new QCheckBox(tr("Use same credentials as target"), m_jumpPanel);
    m_useTargetCredentialsCheck->setChecked(true);

    m_gatewayAuthTypeCombo = new QComboBox(m_jumpPanel);
    m_gatewayAuthTypeCombo->addItem(tr("Password"), static_cast<int>(AuthType::Password));
    m_gatewayAuthTypeCombo->addItem(tr("Private Key"), static_cast<int>(AuthType::PrivateKey));

    m_gatewayPasswordEdit = new QLineEdit(m_jumpPanel);
    m_gatewayPasswordEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_gatewayPasswordEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_gatewayPrivateKeyEdit = new QLineEdit(m_jumpPanel);
    auto *gatewayBrowseButton = new QPushButton(tr("Browse…"), m_jumpPanel);
    auto *gatewayKeyLayout = new QHBoxLayout();
    gatewayKeyLayout->setContentsMargins(0, 0, 0, 0);
    gatewayKeyLayout->addWidget(m_gatewayPrivateKeyEdit, 1);
    gatewayKeyLayout->addWidget(gatewayBrowseButton);
    m_gatewayPrivateKeyRow = new QWidget(m_jumpPanel);
    m_gatewayPrivateKeyRow->setLayout(gatewayKeyLayout);

    m_gatewayPassphraseEdit = new QLineEdit(m_jumpPanel);
    m_gatewayPassphraseEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_gatewayPassphraseEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_gatewayForm->addRow(tr("Host"), m_gatewayHostEdit);
    m_gatewayForm->addRow(tr("Port"), m_gatewayPortSpin);
    m_gatewayForm->addRow(tr("Username"), m_gatewayUsernameEdit);
    m_gatewayForm->addRow(QString(), m_useTargetCredentialsCheck);
    m_gatewayForm->addRow(tr("Authentication"), m_gatewayAuthTypeCombo);
    m_gatewayForm->addRow(tr("Password"), m_gatewayPasswordEdit);
    m_gatewayForm->addRow(tr("Private Key"), m_gatewayPrivateKeyRow);
    m_gatewayForm->addRow(tr("Passphrase"), m_gatewayPassphraseEdit);
    gatewayLayout->addLayout(m_gatewayForm);

    m_commandPanel = new QGroupBox(tr("ProxyCommand"), page);
    auto *commandLayout = new QVBoxLayout(m_commandPanel);
    m_proxyCommandEdit = new QLineEdit(m_commandPanel);
    m_proxyCommandEdit->setPlaceholderText(QStringLiteral("nc -X connect -x 127.0.0.1:9050 %h %p"));
    auto *tokenHint = new QLabel(
        tr("Tokens (expanded by libssh): %%h host, %%p port, %%r user, %%n original host"),
        m_commandPanel);
    tokenHint->setWordWrap(true);
    auto *securityHint = new QLabel(
        tr("Warning: this command runs locally with your user privileges."), m_commandPanel);
    securityHint->setWordWrap(true);
    commandLayout->addWidget(m_proxyCommandEdit);
    commandLayout->addWidget(tokenHint);
    commandLayout->addWidget(securityHint);

    layout->addWidget(modeGroup);
    layout->addWidget(m_jumpPanel);
    layout->addWidget(m_commandPanel);
    layout->addStretch(1);

    connect(m_proxyModeGroup,
            &QButtonGroup::idClicked,
            this,
            &ConnectionDialog::onProxyModeButtonClicked);
    connect(m_gatewayAuthTypeCombo,
            &QComboBox::currentIndexChanged,
            this,
            &ConnectionDialog::onGatewayAuthTypeChanged);
    connect(
        m_hopList, &QListWidget::currentRowChanged, this, &ConnectionDialog::onHopSelectionChanged);
    connect(m_addHopButton, &QPushButton::clicked, this, &ConnectionDialog::onAddHop);
    connect(m_removeHopButton, &QPushButton::clicked, this, &ConnectionDialog::onRemoveHop);
    connect(m_useTargetCredentialsCheck,
            &QCheckBox::toggled,
            this,
            &ConnectionDialog::updateGatewayAuthFieldsVisibility);
    connect(gatewayBrowseButton,
            &QPushButton::clicked,
            this,
            &ConnectionDialog::browseGatewayPrivateKey);

    m_jumpHops.append(JumpHop{});
    refreshHopList();
    if (m_hopList->count() > 0) {
        m_hopList->setCurrentRow(0);
    }

    return page;
}

QWidget *ConnectionDialog::createScpShellPage()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);

    auto *fallbackGroup = new QGroupBox(tr("Fallback"), content);
    auto *fallbackLayout = new QVBoxLayout(fallbackGroup);
    m_allowScpFallbackCheck =
        new QCheckBox(tr("Allow SCP + shell fallback when SFTP is unavailable"), fallbackGroup);
    m_allowScpFallbackCheck->setChecked(true);
    fallbackLayout->addWidget(m_allowScpFallbackCheck);

    auto *shellGroup = new QGroupBox(tr("Shell"), content);
    auto *shellForm = new QFormLayout(shellGroup);
    m_shellEdit = new QLineEdit(shellGroup);
    m_shellEdit->setPlaceholderText(tr("default login shell (e.g. /bin/bash)"));
    shellForm->addRow(tr("Shell"), m_shellEdit);

    auto *listingGroup = new QGroupBox(tr("Directory listing"), content);
    auto *listingForm = new QFormLayout(listingGroup);
    m_listingCommandEdit = new QLineEdit(listingGroup);
    m_listingCommandEdit->setPlaceholderText(QStringLiteral("ls -la"));
    m_ignoreLsWarningsCheck = new QCheckBox(tr("Ignore ls warnings (exit code 1)"), listingGroup);
    m_tryFullTimeCheck = new QCheckBox(tr("Try ls --full-time"), listingGroup);
    m_tryFullTimeCheck->setChecked(true);
    listingForm->addRow(tr("Listing command"), m_listingCommandEdit);
    listingForm->addRow(QString(), m_ignoreLsWarningsCheck);
    listingForm->addRow(QString(), m_tryFullTimeCheck);

    auto *otherGroup = new QGroupBox(tr("Other options"), content);
    auto *otherLayout = new QVBoxLayout(otherGroup);
    m_clearAliasesCheck = new QCheckBox(tr("Clear command aliases on connect"), otherGroup);
    m_clearAliasesCheck->setChecked(true);
    m_clearNationalVarsCheck = new QCheckBox(tr("Clear locale / listing variables"), otherGroup);
    m_clearNationalVarsCheck->setChecked(true);
    otherLayout->addWidget(m_clearAliasesCheck);
    otherLayout->addWidget(m_clearNationalVarsCheck);

    auto *commandsGroup = new QGroupBox(tr("Command templates"), content);
    auto *commandsForm = new QFormLayout(commandsGroup);
    m_mkdirCommandEdit = new QLineEdit(commandsGroup);
    m_mkdirCommandEdit->setPlaceholderText(QStringLiteral("mkdir %1"));
    m_removeCommandEdit = new QLineEdit(commandsGroup);
    m_removeCommandEdit->setPlaceholderText(QStringLiteral("rm -f -r %1"));
    m_renameCommandEdit = new QLineEdit(commandsGroup);
    m_renameCommandEdit->setPlaceholderText(QStringLiteral("mv -f %1 %2"));
    m_realpathCommandEdit = new QLineEdit(commandsGroup);
    m_realpathCommandEdit->setPlaceholderText(QStringLiteral("realpath -e %1"));
    m_resetShellCommandsButton =
        new QPushButton(tr("Reset command set to defaults"), commandsGroup);
    commandsForm->addRow(tr("mkdir (%1 = path)"), m_mkdirCommandEdit);
    commandsForm->addRow(tr("remove (%1 = path)"), m_removeCommandEdit);
    commandsForm->addRow(tr("rename (%1 %2)"), m_renameCommandEdit);
    commandsForm->addRow(tr("realpath (%1 = path)"), m_realpathCommandEdit);
    commandsForm->addRow(QString(), m_resetShellCommandsButton);

    layout->addWidget(fallbackGroup);
    layout->addWidget(shellGroup);
    layout->addWidget(listingGroup);
    layout->addWidget(otherGroup);
    layout->addWidget(commandsGroup);
    layout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll);

    connect(m_resetShellCommandsButton,
            &QPushButton::clicked,
            this,
            &ConnectionDialog::resetShellCommandsToDefaults);

    return page;
}

void ConnectionDialog::setConnection(const Connection &connection)
{
    m_id = connection.id;
    m_nameEdit->setText(connection.name);
    m_hostEdit->setText(connection.host);
    m_portSpin->setValue(connection.port);
    m_usernameEdit->setText(connection.username);

    const int authIndex = m_authTypeCombo->findData(static_cast<int>(connection.authType));
    if (authIndex >= 0) {
        m_authTypeCombo->setCurrentIndex(authIndex);
    }

    m_privateKeyEdit->setText(connection.privateKeyPath);
    m_startupDirEdit->setText(connection.startupDirectory);
    m_passwordEdit->clear();
    m_passphraseEdit->clear();

    m_jumpHops = connection.jumpHops;
    if (m_jumpHops.isEmpty()) {
        m_jumpHops.append(JumpHop{});
    }
    m_proxyCommandEdit->setText(connection.proxyCommand);
    setProxyMode(connection.proxyMode, false);

    refreshHopList();
    if (m_hopList->count() > 0) {
        m_hopList->setCurrentRow(0);
    }
    syncHopEditorFromCurrent();

    m_keepAliveIntervalSpin->setValue(connection.keepAliveIntervalSec);
    m_keepAliveCountSpin->setValue(connection.keepAliveCountMax);
    m_compressionCheck->setChecked(connection.compressionEnabled);
    applyShellCommandsToForm(connection.shellCommands);

    updateAuthFieldsVisibility();
    updateProxyPanelVisibility();
    updateGatewayAuthFieldsVisibility();
}

Connection ConnectionDialog::connection() const
{
    Connection connection;
    connection.id = m_id;
    connection.name = m_nameEdit->text().trimmed();
    connection.host = m_hostEdit->text().trimmed();
    connection.port = static_cast<quint16>(m_portSpin->value());
    connection.username = m_usernameEdit->text().trimmed();
    connection.authType = static_cast<AuthType>(m_authTypeCombo->currentData().toInt());
    connection.privateKeyPath = m_privateKeyEdit->text().trimmed();
    connection.startupDirectory = m_startupDirEdit->text().trimmed();

    connection.proxyMode = m_proxyMode;
    connection.jumpHops.clear();
    connection.proxyCommand.clear();
    if (m_proxyMode == SshProxyMode::ProxyJump) {
        ConnectionDialog *mutableThis = const_cast<ConnectionDialog *>(this);
        mutableThis->syncCurrentHopFromEditor();
        connection.jumpHops = m_jumpHops;
    } else if (m_proxyMode == SshProxyMode::ProxyCommand) {
        connection.proxyCommand = m_proxyCommandEdit->text().trimmed();
    }
    connection.normalizeProxyFields();

    connection.keepAliveIntervalSec = m_keepAliveIntervalSpin->value();
    connection.keepAliveCountMax = m_keepAliveCountSpin->value();
    connection.compressionEnabled = m_compressionCheck->isChecked();
    connection.shellCommands = shellCommandsFromForm();

    return connection;
}

QString ConnectionDialog::password() const
{
    return m_passwordEdit->text();
}
QString ConnectionDialog::passphrase() const
{
    return m_passphraseEdit->text();
}
bool ConnectionDialog::passwordProvided() const
{
    return !m_passwordEdit->text().isEmpty();
}
bool ConnectionDialog::passphraseProvided() const
{
    return !m_passphraseEdit->text().isEmpty();
}

QString ConnectionDialog::gatewayPassword() const
{
    return m_gatewayPasswordEdit->text();
}
QString ConnectionDialog::gatewayPassphrase() const
{
    return m_gatewayPassphraseEdit->text();
}
bool ConnectionDialog::gatewayPasswordProvided() const
{
    return !m_gatewayPasswordEdit->text().isEmpty();
}
bool ConnectionDialog::gatewayPassphraseProvided() const
{
    return !m_gatewayPassphraseEdit->text().isEmpty();
}

void ConnectionDialog::onAuthTypeChanged(int index)
{
    Q_UNUSED(index);
    updateAuthFieldsVisibility();
}

void ConnectionDialog::onGatewayAuthTypeChanged(int index)
{
    Q_UNUSED(index);
    updateGatewayAuthFieldsVisibility();
}

void ConnectionDialog::onProxyModeButtonClicked(int id)
{
    setProxyMode(static_cast<SshProxyMode>(id), true);
}

bool ConnectionDialog::proxyJumpHasData() const
{
    for (const JumpHop &hop : m_jumpHops) {
        if (!hop.host.trimmed().isEmpty() || !hop.username.trimmed().isEmpty() ||
            !hop.privateKeyPath.trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

bool ConnectionDialog::proxyCommandHasData() const
{
    return !m_proxyCommandEdit->text().trimmed().isEmpty();
}

void ConnectionDialog::setProxyMode(SshProxyMode mode, bool confirmClear)
{
    if (confirmClear && mode != m_proxyMode) {
        const bool clearingJump = m_proxyMode == SshProxyMode::ProxyJump &&
                                  mode != SshProxyMode::ProxyJump && proxyJumpHasData();
        const bool clearingCommand = m_proxyMode == SshProxyMode::ProxyCommand &&
                                     mode != SshProxyMode::ProxyCommand && proxyCommandHasData();

        if (clearingJump || clearingCommand) {
            const QString message =
                clearingJump ? tr("Switching mode will clear the configured jump hops. Continue?")
                             : tr("Switching mode will clear the ProxyCommand. Continue?");
            const auto reply = QMessageBox::question(this,
                                                     tr("Change SSH Proxy mode"),
                                                     message,
                                                     QMessageBox::Yes | QMessageBox::No,
                                                     QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                const QSignalBlocker blocker(m_proxyModeGroup);
                if (QAbstractButton *button =
                        m_proxyModeGroup->button(static_cast<int>(m_proxyMode))) {
                    button->setChecked(true);
                }
                return;
            }

            if (clearingJump) {
                syncCurrentHopFromEditor();
                m_jumpHops.clear();
                m_jumpHops.append(JumpHop{});
                refreshHopList();
                if (m_hopList->count() > 0) {
                    m_hopList->setCurrentRow(0);
                }
                syncHopEditorFromCurrent();
            }
            if (clearingCommand) {
                m_proxyCommandEdit->clear();
            }
        }
    }

    m_proxyMode = mode;
    {
        const QSignalBlocker blocker(m_proxyModeGroup);
        if (QAbstractButton *button = m_proxyModeGroup->button(static_cast<int>(mode))) {
            button->setChecked(true);
        }
    }

    if (mode == SshProxyMode::ProxyJump && m_jumpHops.isEmpty()) {
        m_jumpHops.append(JumpHop{});
        refreshHopList();
        m_hopList->setCurrentRow(0);
        syncHopEditorFromCurrent();
    }

    updateProxyPanelVisibility();
}

void ConnectionDialog::onHopSelectionChanged()
{
    syncHopEditorFromCurrent();
}

void ConnectionDialog::onAddHop()
{
    syncCurrentHopFromEditor();
    m_jumpHops.append(JumpHop{});
    refreshHopList();
    m_hopList->setCurrentRow(m_jumpHops.size() - 1);
    syncHopEditorFromCurrent();
}

void ConnectionDialog::onRemoveHop()
{
    const int index = currentHopIndex();
    if (index < 0 || m_jumpHops.size() <= 1) {
        return;
    }
    m_jumpHops.removeAt(index);
    refreshHopList();
    m_hopList->setCurrentRow(qMin(index, m_jumpHops.size() - 1));
    syncHopEditorFromCurrent();
}

void ConnectionDialog::browsePrivateKey()
{
    const QString path =
        QFileDialog::getOpenFileName(this,
                                     tr("Select Private Key"),
                                     QStringLiteral("%1/.ssh").arg(QDir::homePath()),
                                     tr("All Files (*)"));
    if (!path.isEmpty()) {
        m_privateKeyEdit->setText(path);
    }
}

void ConnectionDialog::browseGatewayPrivateKey()
{
    const QString path =
        QFileDialog::getOpenFileName(this,
                                     tr("Select Gateway Private Key"),
                                     QStringLiteral("%1/.ssh").arg(QDir::homePath()),
                                     tr("All Files (*)"));
    if (!path.isEmpty()) {
        m_gatewayPrivateKeyEdit->setText(path);
    }
}

void ConnectionDialog::accept()
{
    if (!validate()) {
        return;
    }
    syncCurrentHopFromEditor();
    QDialog::accept();
}

bool ConnectionDialog::validate()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Name is required."));
        m_shell->selectById(QStringLiteral("session"));
        m_nameEdit->setFocus();
        return false;
    }
    if (m_hostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Host is required."));
        m_shell->selectById(QStringLiteral("session"));
        m_hostEdit->setFocus();
        return false;
    }
    if (m_usernameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Username is required."));
        m_shell->selectById(QStringLiteral("session"));
        m_usernameEdit->setFocus();
        return false;
    }

    const auto authType = static_cast<AuthType>(m_authTypeCombo->currentData().toInt());
    if (authType == AuthType::PrivateKey && m_privateKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Private key path is required."));
        m_shell->selectById(QStringLiteral("session"));
        m_privateKeyEdit->setFocus();
        return false;
    }

    if (m_proxyMode == SshProxyMode::ProxyJump) {
        syncCurrentHopFromEditor();
        for (int i = 0; i < m_jumpHops.size(); ++i) {
            const JumpHop &hop = m_jumpHops.at(i);
            if (hop.host.trimmed().isEmpty()) {
                QMessageBox::warning(
                    this, tr("Validation"), tr("Gateway host is required for hop %1.").arg(i + 1));
                m_shell->selectById(QStringLiteral("ssh-proxy"));
                m_hopList->setCurrentRow(i);
                syncHopEditorFromCurrent();
                m_gatewayHostEdit->setFocus();
                return false;
            }
            if (hop.username.trimmed().isEmpty()) {
                QMessageBox::warning(this,
                                     tr("Validation"),
                                     tr("Gateway username is required for hop %1.").arg(i + 1));
                m_shell->selectById(QStringLiteral("ssh-proxy"));
                m_hopList->setCurrentRow(i);
                syncHopEditorFromCurrent();
                m_gatewayUsernameEdit->setFocus();
                return false;
            }

            if (i == 0 && !hop.useTargetCredentials) {
                const auto gatewayAuth =
                    static_cast<AuthType>(m_gatewayAuthTypeCombo->currentData().toInt());
                if (gatewayAuth == AuthType::PrivateKey && hop.privateKeyPath.trimmed().isEmpty()) {
                    QMessageBox::warning(
                        this, tr("Validation"), tr("Gateway private key path is required."));
                    m_shell->selectById(QStringLiteral("ssh-proxy"));
                    m_gatewayPrivateKeyEdit->setFocus();
                    return false;
                }
                if (gatewayAuth == AuthType::Password && m_mode == Mode::Create &&
                    m_gatewayPasswordEdit->text().isEmpty()) {
                    QMessageBox::warning(
                        this, tr("Validation"), tr("Gateway password is required."));
                    m_shell->selectById(QStringLiteral("ssh-proxy"));
                    m_gatewayPasswordEdit->setFocus();
                    return false;
                }
            }
        }
    } else if (m_proxyMode == SshProxyMode::ProxyCommand) {
        const QString command = m_proxyCommandEdit->text().trimmed();
        if (command.isEmpty() || isSshNoneToken(command)) {
            QMessageBox::warning(this, tr("Validation"), tr("ProxyCommand is required."));
            m_shell->selectById(QStringLiteral("ssh-proxy"));
            m_proxyCommandEdit->setFocus();
            return false;
        }
    }

    return true;
}

void ConnectionDialog::updateAuthFieldsVisibility()
{
    const auto authType = static_cast<AuthType>(m_authTypeCombo->currentData().toInt());
    const bool usePassword = authType == AuthType::Password;

    if (!m_authForm) {
        return;
    }

    for (int i = 0; i < m_authForm->rowCount(); ++i) {
        QLayoutItem *fieldItem = m_authForm->itemAt(i, QFormLayout::FieldRole);
        if (!fieldItem || !fieldItem->widget()) {
            continue;
        }
        QWidget *field = fieldItem->widget();

        if (field == m_passwordEdit) {
            m_authForm->setRowVisible(i, usePassword);
        } else if (field == m_privateKeyRow || field == m_passphraseEdit) {
            m_authForm->setRowVisible(i, !usePassword);
        }
    }
}

void ConnectionDialog::updateGatewayAuthFieldsVisibility()
{
    const bool useTargetCreds = m_useTargetCredentialsCheck->isChecked();
    const auto authType = static_cast<AuthType>(m_gatewayAuthTypeCombo->currentData().toInt());
    const bool usePassword = authType == AuthType::Password;

    if (!m_gatewayForm) {
        return;
    }

    for (int i = 0; i < m_gatewayForm->rowCount(); ++i) {
        QLayoutItem *fieldItem = m_gatewayForm->itemAt(i, QFormLayout::FieldRole);
        if (!fieldItem || !fieldItem->widget()) {
            continue;
        }
        QWidget *field = fieldItem->widget();

        if (field == m_gatewayAuthTypeCombo || field == m_gatewayPasswordEdit ||
            field == m_gatewayPrivateKeyRow || field == m_gatewayPassphraseEdit) {
            const bool showAuth = !useTargetCreds;
            if (field == m_gatewayPasswordEdit) {
                m_gatewayForm->setRowVisible(i, showAuth && usePassword);
            } else if (field == m_gatewayPrivateKeyRow || field == m_gatewayPassphraseEdit) {
                m_gatewayForm->setRowVisible(i, showAuth && !usePassword);
            } else if (field == m_gatewayAuthTypeCombo) {
                m_gatewayForm->setRowVisible(i, showAuth);
            }
        }
    }
}

void ConnectionDialog::updateProxyPanelVisibility()
{
    const bool jump = m_proxyMode == SshProxyMode::ProxyJump;
    const bool command = m_proxyMode == SshProxyMode::ProxyCommand;

    m_jumpPanel->setVisible(jump);
    m_commandPanel->setVisible(command);

    if (jump) {
        m_removeHopButton->setEnabled(m_jumpHops.size() > 1);
        updateGatewayAuthFieldsVisibility();
    }
}

void ConnectionDialog::syncHopEditorFromCurrent()
{
    const int index = currentHopIndex();
    if (index < 0 || index >= m_jumpHops.size()) {
        return;
    }

    const JumpHop &hop = m_jumpHops.at(index);
    m_gatewayHostEdit->setText(hop.host);
    m_gatewayPortSpin->setValue(hop.port);
    m_gatewayUsernameEdit->setText(hop.username);
    m_useTargetCredentialsCheck->setChecked(hop.useTargetCredentials);

    const int authIndex = m_gatewayAuthTypeCombo->findData(static_cast<int>(hop.authType));
    if (authIndex >= 0) {
        m_gatewayAuthTypeCombo->setCurrentIndex(authIndex);
    }
    m_gatewayPrivateKeyEdit->setText(hop.privateKeyPath);
    m_gatewayPasswordEdit->clear();
    m_gatewayPassphraseEdit->clear();

    updateGatewayAuthFieldsVisibility();
}

void ConnectionDialog::syncCurrentHopFromEditor()
{
    const int index = currentHopIndex();
    if (index < 0 || index >= m_jumpHops.size()) {
        return;
    }

    JumpHop &hop = m_jumpHops[index];
    hop.host = m_gatewayHostEdit->text().trimmed();
    hop.port = static_cast<quint16>(m_gatewayPortSpin->value());
    hop.username = m_gatewayUsernameEdit->text().trimmed();
    hop.useTargetCredentials = m_useTargetCredentialsCheck->isChecked();
    hop.authType = static_cast<AuthType>(m_gatewayAuthTypeCombo->currentData().toInt());
    hop.privateKeyPath = m_gatewayPrivateKeyEdit->text().trimmed();
    refreshHopList();
}

void ConnectionDialog::refreshHopList()
{
    const int previousRow = m_hopList->currentRow();
    m_hopList->clear();
    for (int i = 0; i < m_jumpHops.size(); ++i) {
        const JumpHop &hop = m_jumpHops.at(i);
        const QString label = hop.host.isEmpty()
                                  ? tr("Hop %1").arg(i + 1)
                                  : tr("Hop %1 — %2@%3").arg(i + 1).arg(hop.username, hop.host);
        m_hopList->addItem(label);
    }
    if (m_hopList->count() > 0) {
        m_hopList->setCurrentRow(qBound(0, previousRow, m_hopList->count() - 1));
    }
    m_removeHopButton->setEnabled(m_proxyMode == SshProxyMode::ProxyJump && m_jumpHops.size() > 1);
}

int ConnectionDialog::currentHopIndex() const
{
    return m_hopList ? m_hopList->currentRow() : -1;
}

void ConnectionDialog::applyShellCommandsToForm(const ShellCommandSetConfig &config)
{
    m_allowScpFallbackCheck->setChecked(config.allowScpFallback);
    m_shellEdit->setText(config.shell);
    m_listingCommandEdit->setText(config.listingCommand);
    m_clearAliasesCheck->setChecked(config.clearAliases);
    m_clearNationalVarsCheck->setChecked(config.clearNationalVars);
    m_tryFullTimeCheck->setChecked(config.tryFullTime);
    m_ignoreLsWarningsCheck->setChecked(config.ignoreLsWarnings);
    m_mkdirCommandEdit->setText(config.mkdirCommand);
    m_removeCommandEdit->setText(config.removeCommand);
    m_renameCommandEdit->setText(config.renameCommand);
    m_realpathCommandEdit->setText(config.realpathCommand);
}

ShellCommandSetConfig ConnectionDialog::shellCommandsFromForm() const
{
    ShellCommandSetConfig config;
    config.allowScpFallback = m_allowScpFallbackCheck->isChecked();
    config.shell = m_shellEdit->text().trimmed();
    config.listingCommand = m_listingCommandEdit->text().trimmed();
    config.clearAliases = m_clearAliasesCheck->isChecked();
    config.clearNationalVars = m_clearNationalVarsCheck->isChecked();
    config.tryFullTime = m_tryFullTimeCheck->isChecked();
    config.ignoreLsWarnings = m_ignoreLsWarningsCheck->isChecked();
    config.mkdirCommand = m_mkdirCommandEdit->text().trimmed();
    config.removeCommand = m_removeCommandEdit->text().trimmed();
    config.renameCommand = m_renameCommandEdit->text().trimmed();
    config.realpathCommand = m_realpathCommandEdit->text().trimmed();
    return config;
}

void ConnectionDialog::resetShellCommandsToDefaults()
{
    applyShellCommandsToForm(ShellCommandSetConfig{});
}
