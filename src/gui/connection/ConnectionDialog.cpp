// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_id(QUuid::createUuid())
{
    setupUi();
    setWindowTitle(mode == Mode::Create ? tr("New Connection") : tr("Edit Connection"));
    resize(520, 560);
}

void ConnectionDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *targetGroup = new QGroupBox(tr("Target"), this);
    m_targetForm = new QFormLayout(targetGroup);

    m_nameEdit = new QLineEdit(this);
    m_hostEdit = new QLineEdit(this);
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(22);
    m_usernameEdit = new QLineEdit(this);

    m_authTypeCombo = new QComboBox(this);
    m_authTypeCombo->addItem(tr("Password"), static_cast<int>(AuthType::Password));
    m_authTypeCombo->addItem(tr("Private Key"), static_cast<int>(AuthType::PrivateKey));

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_passwordEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_privateKeyEdit = new QLineEdit(this);
    auto *browseButton = new QPushButton(tr("Browse…"), this);
    auto *keyLayout = new QHBoxLayout();
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(m_privateKeyEdit, 1);
    keyLayout->addWidget(browseButton);
    m_privateKeyRow = new QWidget(this);
    m_privateKeyRow->setLayout(keyLayout);

    m_passphraseEdit = new QLineEdit(this);
    m_passphraseEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_passphraseEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_startupDirEdit = new QLineEdit(this);
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

    m_gatewayGroup = new QGroupBox(tr("Gateway / Jump Host"), this);
    auto *gatewayLayout = new QVBoxLayout(m_gatewayGroup);

    m_useGatewayCheck = new QCheckBox(tr("Connect via gateway"), this);
    gatewayLayout->addWidget(m_useGatewayCheck);

    auto *hint = new QLabel(tr("Route: local → gateway → target"), this);
    hint->setWordWrap(true);
    gatewayLayout->addWidget(hint);

    auto *hopButtonsLayout = new QHBoxLayout();
    m_hopList = new QListWidget(this);
    m_hopList->setMaximumHeight(72);
    m_addHopButton = new QPushButton(tr("Add hop"), this);
    m_removeHopButton = new QPushButton(tr("Remove hop"), this);
    hopButtonsLayout->addWidget(m_addHopButton);
    hopButtonsLayout->addWidget(m_removeHopButton);
    hopButtonsLayout->addStretch(1);
    gatewayLayout->addWidget(m_hopList);
    gatewayLayout->addLayout(hopButtonsLayout);

    m_gatewayForm = new QFormLayout();
    m_gatewayHostEdit = new QLineEdit(this);
    m_gatewayPortSpin = new QSpinBox(this);
    m_gatewayPortSpin->setRange(1, 65535);
    m_gatewayPortSpin->setValue(22);
    m_gatewayUsernameEdit = new QLineEdit(this);
    m_useTargetCredentialsCheck = new QCheckBox(tr("Use same credentials as target"), this);
    m_useTargetCredentialsCheck->setChecked(true);

    m_gatewayAuthTypeCombo = new QComboBox(this);
    m_gatewayAuthTypeCombo->addItem(tr("Password"), static_cast<int>(AuthType::Password));
    m_gatewayAuthTypeCombo->addItem(tr("Private Key"), static_cast<int>(AuthType::PrivateKey));

    m_gatewayPasswordEdit = new QLineEdit(this);
    m_gatewayPasswordEdit->setEchoMode(QLineEdit::Password);
    if (m_mode == Mode::Edit) {
        m_gatewayPasswordEdit->setPlaceholderText(tr("Leave blank to keep existing"));
    }

    m_gatewayPrivateKeyEdit = new QLineEdit(this);
    auto *gatewayBrowseButton = new QPushButton(tr("Browse…"), this);
    auto *gatewayKeyLayout = new QHBoxLayout();
    gatewayKeyLayout->setContentsMargins(0, 0, 0, 0);
    gatewayKeyLayout->addWidget(m_gatewayPrivateKeyEdit, 1);
    gatewayKeyLayout->addWidget(gatewayBrowseButton);
    m_gatewayPrivateKeyRow = new QWidget(this);
    m_gatewayPrivateKeyRow->setLayout(gatewayKeyLayout);

    m_gatewayPassphraseEdit = new QLineEdit(this);
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

    m_advancedGroup = new QGroupBox(tr("Advanced"), this);
    auto *advancedForm = new QFormLayout(m_advancedGroup);

    m_keepAliveIntervalSpin = new QSpinBox(this);
    m_keepAliveIntervalSpin->setRange(0, 3600);
    m_keepAliveIntervalSpin->setSuffix(tr(" s"));
    m_keepAliveIntervalSpin->setSpecialValueText(tr("Disabled"));
    m_keepAliveIntervalSpin->setValue(0);

    m_keepAliveCountSpin = new QSpinBox(this);
    m_keepAliveCountSpin->setRange(1, 10);
    m_keepAliveCountSpin->setValue(3);
    m_keepAliveCountSpin->setEnabled(false);

    m_compressionCheck = new QCheckBox(tr("Enable SSH compression"), this);

    advancedForm->addRow(tr("Keep-alive interval"), m_keepAliveIntervalSpin);
    advancedForm->addRow(tr("Keep-alive max retries"), m_keepAliveCountSpin);
    advancedForm->addRow(QString(), m_compressionCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addWidget(targetGroup);
    layout->addWidget(m_gatewayGroup);
    layout->addWidget(m_advancedGroup);
    layout->addWidget(buttons);

    connect(m_authTypeCombo,
            &QComboBox::currentIndexChanged,
            this,
            &ConnectionDialog::onAuthTypeChanged);
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
    connect(m_keepAliveIntervalSpin, &QSpinBox::valueChanged, this, [this](int value) {
        m_keepAliveCountSpin->setEnabled(value > 0);
    });
    connect(browseButton, &QPushButton::clicked, this, &ConnectionDialog::browsePrivateKey);
    connect(gatewayBrowseButton,
            &QPushButton::clicked,
            this,
            &ConnectionDialog::browseGatewayPrivateKey);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateAuthFieldsVisibility();
    updateGatewayPanelVisibility();
    updateGatewayAuthFieldsVisibility();
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
        m_nameEdit->setFocus();
        return false;
    }
    if (m_hostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Host is required."));
        m_hostEdit->setFocus();
        return false;
    }
    if (m_usernameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Username is required."));
        m_usernameEdit->setFocus();
        return false;
    }

    const auto authType = static_cast<AuthType>(m_authTypeCombo->currentData().toInt());
    if (authType == AuthType::PrivateKey && m_privateKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"), tr("Private key path is required."));
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
                m_hopList->setCurrentRow(i);
                syncHopEditorFromCurrent();
                m_gatewayHostEdit->setFocus();
                return false;
            }
            if (hop.username.trimmed().isEmpty()) {
                QMessageBox::warning(this,
                                     tr("Validation"),
                                     tr("Gateway username is required for hop %1.").arg(i + 1));
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
                    m_gatewayPrivateKeyEdit->setFocus();
                    return false;
                }
                if (gatewayAuth == AuthType::Password && m_mode == Mode::Create &&
                    m_gatewayPasswordEdit->text().isEmpty()) {
                    QMessageBox::warning(
                        this, tr("Validation"), tr("Gateway password is required."));
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
