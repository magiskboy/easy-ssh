// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionDialog.h"

#include "gui/widgets/CategoryDialogShell.h"

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
#include <QScrollArea>
#include <QSpinBox>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_id(QUuid::createUuid())
{
    setupUi();
    setWindowTitle(mode == Mode::Create ? tr("New Connection") : tr("Edit Connection"));
    resize(640, 480);
}

void ConnectionDialog::setupUi()
{
    m_shell = new CategoryDialogShell(this);
    m_shell->addPage(nullptr, tr("Session"), createSessionPage(), QStringLiteral("session"));

    QTreeWidgetItem *connectionItem = m_shell->addPage(
        nullptr, tr("Connection"), createConnectionPage(), QStringLiteral("connection"));
    m_shell->addPage(connectionItem, tr("Tunnel"), createTunnelPage(), QStringLiteral("tunnel"));

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
    updateGatewayPanelVisibility();
    updateGatewayAuthFieldsVisibility();
}

QWidget *ConnectionDialog::createSessionPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *targetGroup = new QGroupBox(tr("Target"), page);
    m_targetForm = new QFormLayout(targetGroup);

    m_nameEdit = new QLineEdit(targetGroup);
    m_hostEdit = new QLineEdit(targetGroup);
    m_portSpin = new QSpinBox(targetGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(22);
    m_usernameEdit = new QLineEdit(targetGroup);

    m_authTypeCombo = new QComboBox(targetGroup);
    m_authTypeCombo->addItem(tr("Password"), static_cast<int>(AuthType::Password));
    m_authTypeCombo->addItem(tr("Private Key"), static_cast<int>(AuthType::PrivateKey));

    m_passwordEdit = new QLineEdit(targetGroup);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_passwordEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_privateKeyEdit = new QLineEdit(targetGroup);
    auto *browseButton = new QPushButton(tr("Browse…"), targetGroup);
    auto *keyLayout = new QHBoxLayout();
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(m_privateKeyEdit, 1);
    keyLayout->addWidget(browseButton);
    m_privateKeyRow = new QWidget(targetGroup);
    m_privateKeyRow->setLayout(keyLayout);

    m_passphraseEdit = new QLineEdit(targetGroup);
    m_passphraseEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_passphraseEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_startupDirEdit = new QLineEdit(targetGroup);
    m_startupDirEdit->setPlaceholderText(tr("Optional"));

    m_targetForm->addRow(tr("Name"), m_nameEdit);
    m_targetForm->addRow(tr("Host"), m_hostEdit);
    m_targetForm->addRow(tr("Port"), m_portSpin);
    m_targetForm->addRow(tr("Username"), m_usernameEdit);
    m_targetForm->addRow(tr("Authentication"), m_authTypeCombo);
    m_targetForm->addRow(tr("Password"), m_passwordEdit);
    m_targetForm->addRow(tr("Private Key"), m_privateKeyRow);
    m_targetForm->addRow(tr("Passphrase"), m_passphraseEdit);
    m_targetForm->addRow(tr("Startup Directory"), m_startupDirEdit);

    layout->addWidget(targetGroup);
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

QWidget *ConnectionDialog::createTunnelPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *gatewayGroup = new QGroupBox(tr("Gateway / Jump Host"), page);
    auto *gatewayLayout = new QVBoxLayout(gatewayGroup);

    m_useGatewayCheck = new QCheckBox(tr("Connect via gateway"), gatewayGroup);
    gatewayLayout->addWidget(m_useGatewayCheck);

    auto *hint = new QLabel(tr("Route: local → gateway → target"), gatewayGroup);
    hint->setWordWrap(true);
    gatewayLayout->addWidget(hint);

    auto *hopButtonsLayout = new QHBoxLayout();
    m_hopList = new QListWidget(gatewayGroup);
    m_hopList->setMaximumHeight(72);
    m_addHopButton = new QPushButton(tr("Add hop"), gatewayGroup);
    m_removeHopButton = new QPushButton(tr("Remove hop"), gatewayGroup);
    hopButtonsLayout->addWidget(m_addHopButton);
    hopButtonsLayout->addWidget(m_removeHopButton);
    hopButtonsLayout->addStretch(1);
    gatewayLayout->addWidget(m_hopList);
    gatewayLayout->addLayout(hopButtonsLayout);

    m_gatewayForm = new QFormLayout();
    m_gatewayHostEdit = new QLineEdit(gatewayGroup);
    m_gatewayPortSpin = new QSpinBox(gatewayGroup);
    m_gatewayPortSpin->setRange(1, 65535);
    m_gatewayPortSpin->setValue(22);
    m_gatewayUsernameEdit = new QLineEdit(gatewayGroup);
    m_useTargetCredentialsCheck = new QCheckBox(tr("Use same credentials as target"), gatewayGroup);
    m_useTargetCredentialsCheck->setChecked(true);

    m_gatewayAuthTypeCombo = new QComboBox(gatewayGroup);
    m_gatewayAuthTypeCombo->addItem(tr("Password"), static_cast<int>(AuthType::Password));
    m_gatewayAuthTypeCombo->addItem(tr("Private Key"), static_cast<int>(AuthType::PrivateKey));

    m_gatewayPasswordEdit = new QLineEdit(gatewayGroup);
    m_gatewayPasswordEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_gatewayPasswordEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_gatewayPrivateKeyEdit = new QLineEdit(gatewayGroup);
    auto *gatewayBrowseButton = new QPushButton(tr("Browse…"), gatewayGroup);
    auto *gatewayKeyLayout = new QHBoxLayout();
    gatewayKeyLayout->setContentsMargins(0, 0, 0, 0);
    gatewayKeyLayout->addWidget(m_gatewayPrivateKeyEdit, 1);
    gatewayKeyLayout->addWidget(gatewayBrowseButton);
    m_gatewayPrivateKeyRow = new QWidget(gatewayGroup);
    m_gatewayPrivateKeyRow->setLayout(gatewayKeyLayout);

    m_gatewayPassphraseEdit = new QLineEdit(gatewayGroup);
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

    layout->addWidget(gatewayGroup);
    layout->addStretch(1);

    connect(m_gatewayAuthTypeCombo,
            &QComboBox::currentIndexChanged,
            this,
            &ConnectionDialog::onGatewayAuthTypeChanged);
    connect(m_useGatewayCheck, &QCheckBox::toggled, this, &ConnectionDialog::onUseGatewayToggled);
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
    m_useGatewayCheck->setChecked(!m_jumpHops.isEmpty());
    if (m_jumpHops.isEmpty()) {
        JumpHop defaultHop;
        m_jumpHops.append(defaultHop);
    }
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
    updateGatewayPanelVisibility();
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

    connection.jumpHops.clear();
    if (m_useGatewayCheck->isChecked()) {
        ConnectionDialog *mutableThis = const_cast<ConnectionDialog *>(this);
        mutableThis->syncCurrentHopFromEditor();
        connection.jumpHops = m_jumpHops;
    }

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

void ConnectionDialog::onUseGatewayToggled(bool enabled)
{
    Q_UNUSED(enabled);
    if (m_useGatewayCheck->isChecked() && m_jumpHops.isEmpty()) {
        m_jumpHops.append(JumpHop{});
        refreshHopList();
        m_hopList->setCurrentRow(0);
        syncHopEditorFromCurrent();
    }
    updateGatewayPanelVisibility();
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

    if (m_useGatewayCheck->isChecked()) {
        syncCurrentHopFromEditor();
        for (int i = 0; i < m_jumpHops.size(); ++i) {
            const JumpHop &hop = m_jumpHops.at(i);
            if (hop.host.trimmed().isEmpty()) {
                QMessageBox::warning(
                    this, tr("Validation"), tr("Gateway host is required for hop %1.").arg(i + 1));
                m_shell->selectById(QStringLiteral("tunnel"));
                m_hopList->setCurrentRow(i);
                syncHopEditorFromCurrent();
                m_gatewayHostEdit->setFocus();
                return false;
            }
            if (hop.username.trimmed().isEmpty()) {
                QMessageBox::warning(this,
                                     tr("Validation"),
                                     tr("Gateway username is required for hop %1.").arg(i + 1));
                m_shell->selectById(QStringLiteral("tunnel"));
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
                    m_shell->selectById(QStringLiteral("tunnel"));
                    m_gatewayPrivateKeyEdit->setFocus();
                    return false;
                }
                if (gatewayAuth == AuthType::Password && m_mode == Mode::Create &&
                    m_gatewayPasswordEdit->text().isEmpty()) {
                    QMessageBox::warning(
                        this, tr("Validation"), tr("Gateway password is required."));
                    m_shell->selectById(QStringLiteral("tunnel"));
                    m_gatewayPasswordEdit->setFocus();
                    return false;
                }
            }
        }
    }

    return true;
}

void ConnectionDialog::updateAuthFieldsVisibility()
{
    const auto authType = static_cast<AuthType>(m_authTypeCombo->currentData().toInt());
    const bool usePassword = authType == AuthType::Password;

    if (!m_targetForm) {
        return;
    }

    for (int i = 0; i < m_targetForm->rowCount(); ++i) {
        QLayoutItem *fieldItem = m_targetForm->itemAt(i, QFormLayout::FieldRole);
        if (!fieldItem || !fieldItem->widget()) {
            continue;
        }
        QWidget *field = fieldItem->widget();

        if (field == m_passwordEdit) {
            m_targetForm->setRowVisible(i, usePassword);
        } else if (field == m_privateKeyRow || field == m_passphraseEdit) {
            m_targetForm->setRowVisible(i, !usePassword);
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

void ConnectionDialog::updateGatewayPanelVisibility()
{
    const bool enabled = m_useGatewayCheck->isChecked();
    m_hopList->setEnabled(enabled);
    m_addHopButton->setEnabled(enabled);
    m_removeHopButton->setEnabled(enabled && m_jumpHops.size() > 1);

    for (int i = 0; i < m_gatewayForm->rowCount(); ++i) {
        m_gatewayForm->setRowVisible(i, enabled);
    }

    if (enabled) {
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
    m_removeHopButton->setEnabled(m_useGatewayCheck->isChecked() && m_jumpHops.size() > 1);
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
