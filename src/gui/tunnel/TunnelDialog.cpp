// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "TunnelDialog.h"

#include "core/connection/SecretStore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

TunnelDialog::TunnelDialog(Mode mode, const QUuid &connectionId, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_id(QUuid::createUuid()), m_connectionId(connectionId)
{
    setupUi();
    setWindowTitle(mode == Mode::Create ? tr("New Tunnel") : tr("Edit Tunnel"));
    resize(520, 420);
}

void TunnelDialog::setSecretStore(SecretStore *secretStore)
{
    m_secretStore = secretStore;
}

QWidget *TunnelDialog::makeSocketPathRow(QLineEdit *edit, QPushButton *browseButton)
{
    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(edit, 1);
    layout->addWidget(browseButton);
    return row;
}

QString TunnelDialog::chooseUnixSocketPath(QLineEdit *edit, const QString &caption)
{
    QFileDialog dialog(this, caption);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOption(QFileDialog::DontConfirmOverwrite);
    dialog.setNameFilter(tr("All files (*)"));

    const QFileInfo info(edit ? edit->text().trimmed() : QString());
    if (info.isAbsolute()) {
        dialog.setDirectory(info.absolutePath());
        if (!info.fileName().isEmpty()) {
            dialog.selectFile(info.fileName());
        }
    } else {
        dialog.setDirectory(QStringLiteral("/var/run"));
    }

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    const QStringList files = dialog.selectedFiles();
    return files.isEmpty() ? QString() : files.first();
}

void TunnelDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    m_form = new QFormLayout();

    m_nameEdit = new QLineEdit(this);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Local Port Forward"), static_cast<int>(TunnelType::Local));
    m_typeCombo->addItem(tr("Remote Port Forward"), static_cast<int>(TunnelType::Remote));
    m_typeCombo->addItem(tr("Dynamic (SOCKS5)"), static_cast<int>(TunnelType::Dynamic));

    m_localKindCombo = new QComboBox(this);
    m_localKindCombo->addItem(tr("TCP"), static_cast<int>(TunnelEndpointKind::Tcp));
    if (localUnixSocketSupported()) {
        m_localKindCombo->addItem(tr("Unix socket"),
                                  static_cast<int>(TunnelEndpointKind::UnixSocket));
    }

    m_localHostEdit = new QLineEdit(this);
    m_localHostEdit->setText(QStringLiteral("127.0.0.1"));
    m_localPortSpin = new QSpinBox(this);
    m_localPortSpin->setRange(1, 65535);
    m_localPortSpin->setValue(3306);
    m_localSocketEdit = new QLineEdit(this);
    m_localSocketEdit->setPlaceholderText(QStringLiteral("/tmp/easy-ssh.sock"));
    m_localSocketBrowse = new QPushButton(tr("Browse…"), this);
    m_localSocketRow = makeSocketPathRow(m_localSocketEdit, m_localSocketBrowse);

    m_remoteKindCombo = new QComboBox(this);
    m_remoteKindCombo->addItem(tr("TCP"), static_cast<int>(TunnelEndpointKind::Tcp));
    m_remoteKindCombo->addItem(tr("Unix socket"), static_cast<int>(TunnelEndpointKind::UnixSocket));

    m_remoteHostEdit = new QLineEdit(this);
    m_remoteHostEdit->setText(QStringLiteral("127.0.0.1"));
    m_remotePortSpin = new QSpinBox(this);
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(3306);
    m_remoteSocketEdit = new QLineEdit(this);
    m_remoteSocketEdit->setPlaceholderText(QStringLiteral("/var/run/docker.sock"));
    m_remoteSocketBrowse = new QPushButton(tr("Browse…"), this);
    m_remoteSocketBrowse->setToolTip(
        tr("Pick a path on this machine as a template. The tunnel uses the path on the SSH "
           "server — edit the text if the remote path differs."));
    m_remoteSocketRow = makeSocketPathRow(m_remoteSocketEdit, m_remoteSocketBrowse);

    m_socksAuthCombo = new QComboBox(this);
    m_socksAuthCombo->addItem(tr("No authentication"), static_cast<int>(SocksAuthMode::None));
    m_socksAuthCombo->addItem(tr("Username / password"),
                              static_cast<int>(SocksAuthMode::UsernamePassword));
    m_socksUserEdit = new QLineEdit(this);
    m_socksPasswordEdit = new QLineEdit(this);
    m_socksPasswordEdit->setEchoMode(QLineEdit::Password);
    connect(m_socksPasswordEdit, &QLineEdit::textEdited, this, [this]() {
        if (!m_loadingSocksPassword) {
            m_socksPasswordChanged = true;
        }
    });

    m_enabledCheck = new QCheckBox(tr("Enable when session connects"), this);
    m_enabledCheck->setChecked(true);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));

    m_form->addRow(tr("Name"), m_nameEdit);
    m_form->addRow(tr("Type"), m_typeCombo);
    m_form->addRow(tr("Local endpoint"), m_localKindCombo);
    m_form->addRow(tr("Local Host"), m_localHostEdit);
    m_form->addRow(tr("Local Port"), m_localPortSpin);
    m_form->addRow(tr("Local socket path"), m_localSocketRow);
    m_form->addRow(tr("Remote endpoint"), m_remoteKindCombo);
    m_form->addRow(tr("Remote Host"), m_remoteHostEdit);
    m_form->addRow(tr("Remote Port"), m_remotePortSpin);
    m_form->addRow(tr("Remote socket path"), m_remoteSocketRow);
    m_form->addRow(tr("SOCKS authentication"), m_socksAuthCombo);
    m_form->addRow(tr("SOCKS username"), m_socksUserEdit);
    m_form->addRow(tr("SOCKS password"), m_socksPasswordEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addLayout(m_form);
    layout->addWidget(m_enabledCheck);
    layout->addWidget(m_hintLabel);
    layout->addWidget(buttons);

    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, &TunnelDialog::onTypeChanged);
    connect(
        m_localKindCombo, &QComboBox::currentIndexChanged, this, &TunnelDialog::onLocalKindChanged);
    connect(m_remoteKindCombo,
            &QComboBox::currentIndexChanged,
            this,
            &TunnelDialog::onRemoteKindChanged);
    connect(
        m_socksAuthCombo, &QComboBox::currentIndexChanged, this, &TunnelDialog::onSocksAuthChanged);
    connect(m_localSocketBrowse, &QPushButton::clicked, this, &TunnelDialog::browseLocalSocketPath);
    connect(
        m_remoteSocketBrowse, &QPushButton::clicked, this, &TunnelDialog::browseRemoteSocketPath);
    connect(buttons, &QDialogButtonBox::accepted, this, &TunnelDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateFieldVisibility();
}

bool TunnelDialog::localUnixSocketSupported() const
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

void TunnelDialog::browseLocalSocketPath()
{
    const QString path = chooseUnixSocketPath(m_localSocketEdit, tr("Select local Unix socket"));
    if (!path.isEmpty()) {
        m_localSocketEdit->setText(path);
    }
}

void TunnelDialog::browseRemoteSocketPath()
{
    const QString path =
        chooseUnixSocketPath(m_remoteSocketEdit, tr("Select remote Unix socket path"));
    if (!path.isEmpty()) {
        m_remoteSocketEdit->setText(path);
    }
}

void TunnelDialog::setEndpointKindCombo(QComboBox *combo, TunnelEndpointKind kind)
{
    if (combo == nullptr) {
        return;
    }
    const int index = combo->findData(static_cast<int>(kind));
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else {
        combo->setCurrentIndex(0);
    }
}

TunnelEndpointKind TunnelDialog::endpointKindFromCombo(const QComboBox *combo) const
{
    if (combo == nullptr) {
        return TunnelEndpointKind::Tcp;
    }
    return static_cast<TunnelEndpointKind>(combo->currentData().toInt());
}

void TunnelDialog::setTunnel(const TunnelDefinition &tunnel)
{
    m_id = tunnel.id;
    m_connectionId = tunnel.connectionId;
    m_nameEdit->setText(tunnel.name);

    const int typeIndex = m_typeCombo->findData(static_cast<int>(tunnel.type));
    if (typeIndex >= 0) {
        m_typeCombo->setCurrentIndex(typeIndex);
    }

    setEndpointKindCombo(m_localKindCombo, tunnel.localKind);
    m_localHostEdit->setText(tunnel.localHost);
    m_localPortSpin->setValue(tunnel.localPort > 0 ? tunnel.localPort : 3306);
    m_localSocketEdit->setText(tunnel.localSocketPath);

    setEndpointKindCombo(m_remoteKindCombo, tunnel.remoteKind);
    m_remoteHostEdit->setText(tunnel.remoteHost);
    m_remotePortSpin->setValue(tunnel.remotePort > 0 ? tunnel.remotePort : 3306);
    m_remoteSocketEdit->setText(tunnel.remoteSocketPath);

    const int socksIndex = m_socksAuthCombo->findData(static_cast<int>(tunnel.socksAuth));
    if (socksIndex >= 0) {
        m_socksAuthCombo->setCurrentIndex(socksIndex);
    }
    m_socksUserEdit->setText(tunnel.socksUsername);
    m_socksPasswordEdit->clear();
    m_socksPasswordChanged = false;
    m_enabledCheck->setChecked(tunnel.enabled);

    if (m_secretStore && tunnel.socksAuth == SocksAuthMode::UsernamePassword &&
        !tunnel.id.isNull()) {
        m_loadingSocksPassword = true;
        connect(
            m_secretStore,
            &SecretStore::readFinished,
            this,
            [this, tunnelId = tunnel.id](const QUuid &id,
                                         SecretStore::Kind kind,
                                         const QString &value,
                                         bool ok,
                                         const QString &) {
                if (id != tunnelId || kind != SecretStore::Kind::TunnelSocksPassword || !ok) {
                    return;
                }
                m_socksPasswordEdit->setText(value);
                m_socksPasswordChanged = false;
                m_loadingSocksPassword = false;
            },
            Qt::SingleShotConnection);
        m_secretStore->readSecret(tunnel.id, SecretStore::Kind::TunnelSocksPassword);
    }

    updateFieldVisibility();
}

TunnelDefinition TunnelDialog::tunnel() const
{
    TunnelDefinition def;
    def.id = m_id;
    def.connectionId = m_connectionId;
    def.name = m_nameEdit->text().trimmed();
    def.type = static_cast<TunnelType>(m_typeCombo->currentData().toInt());
    def.enabled = m_enabledCheck->isChecked();

    def.localKind = endpointKindFromCombo(m_localKindCombo);
    def.localHost = m_localHostEdit->text().trimmed();
    def.localPort = static_cast<quint16>(m_localPortSpin->value());
    def.localSocketPath = m_localSocketEdit->text().trimmed();

    def.remoteKind = endpointKindFromCombo(m_remoteKindCombo);
    def.remoteHost = m_remoteHostEdit->text().trimmed();
    def.remotePort = static_cast<quint16>(m_remotePortSpin->value());
    def.remoteSocketPath = m_remoteSocketEdit->text().trimmed();

    def.socksAuth = static_cast<SocksAuthMode>(m_socksAuthCombo->currentData().toInt());
    def.socksUsername = m_socksUserEdit->text().trimmed();

    if (def.type == TunnelType::Remote) {
        def.remoteKind = TunnelEndpointKind::Tcp;
    }
    if (def.type == TunnelType::Dynamic) {
        def.localKind = TunnelEndpointKind::Tcp;
        def.remoteKind = TunnelEndpointKind::Tcp;
        def.remotePort = 0;
        def.remoteHost.clear();
        def.remoteSocketPath.clear();
        def.localSocketPath.clear();
    }
    if (def.localKind == TunnelEndpointKind::UnixSocket) {
        def.localPort = 0;
    }
    if (def.remoteKind == TunnelEndpointKind::UnixSocket) {
        def.remotePort = 0;
    }
    if (def.type != TunnelType::Dynamic) {
        def.socksAuth = SocksAuthMode::None;
        def.socksUsername.clear();
    }
    return def;
}

QString TunnelDialog::socksPassword() const
{
    return m_socksPasswordEdit->text();
}

void TunnelDialog::onTypeChanged(int)
{
    updateFieldVisibility();
}

void TunnelDialog::onLocalKindChanged(int)
{
    updateFieldVisibility();
}

void TunnelDialog::onRemoteKindChanged(int)
{
    updateFieldVisibility();
}

void TunnelDialog::onSocksAuthChanged(int)
{
    updateFieldVisibility();
}

void TunnelDialog::updateFieldLabels()
{
    const auto type = static_cast<TunnelType>(m_typeCombo->currentData().toInt());
    auto setLabel = [this](QWidget *field, const QString &text) {
        if (auto *label = qobject_cast<QLabel *>(m_form->labelForField(field))) {
            label->setText(text);
        }
    };

    if (type == TunnelType::Remote) {
        setLabel(m_localKindCombo, tr("Local destination"));
        setLabel(m_localHostEdit, tr("Local Host (destination)"));
        setLabel(m_localPortSpin, tr("Local Port (destination)"));
        setLabel(m_localSocketRow, tr("Local socket path"));
        setLabel(m_remoteHostEdit, tr("Remote Listen Host"));
        setLabel(m_remotePortSpin, tr("Remote Listen Port"));
        m_hintLabel->setText(
            tr("SSH server listens on TCP and forwards connections to this machine "
               "(TCP or a local Unix socket). Binding beyond 127.0.0.1 on the server "
               "may require GatewayPorts."));
    } else if (type == TunnelType::Dynamic) {
        setLabel(m_localHostEdit, tr("Local Bind Host"));
        setLabel(m_localPortSpin, tr("Local Bind Port"));
        m_hintLabel->setText(
            tr("SOCKS5 proxy on the local bind address. Configure clients with "
               "socks5://127.0.0.1:<port>. Destinations are opened dynamically over SSH."));
    } else {
        setLabel(m_localKindCombo, tr("Local endpoint"));
        setLabel(m_localHostEdit, tr("Local Bind Host"));
        setLabel(m_localPortSpin, tr("Local Bind Port"));
        setLabel(m_localSocketRow, tr("Local socket path"));
        setLabel(m_remoteKindCombo, tr("Remote endpoint"));
        setLabel(m_remoteHostEdit, tr("Remote Host (destination)"));
        setLabel(m_remotePortSpin, tr("Remote Port (destination)"));
        setLabel(m_remoteSocketRow, tr("Remote socket path"));
        m_hintLabel->setText(
            tr("Connect your client to the local bind address (TCP or Unix socket); "
               "traffic goes through SSH to the remote TCP host or Unix socket "
               "(e.g. /var/run/docker.sock). Requires OpenSSH StreamLocal for remote sockets."));
    }
}

void TunnelDialog::updateFieldVisibility()
{
    const auto type = static_cast<TunnelType>(m_typeCombo->currentData().toInt());
    const bool isDynamic = type == TunnelType::Dynamic;
    const bool isRemote = type == TunnelType::Remote;
    const bool isLocal = type == TunnelType::Local;

    const TunnelEndpointKind localKind = endpointKindFromCombo(m_localKindCombo);
    const TunnelEndpointKind remoteKind = endpointKindFromCombo(m_remoteKindCombo);
    const bool localTcp = localKind == TunnelEndpointKind::Tcp;
    const bool remoteTcp = remoteKind == TunnelEndpointKind::Tcp;
    const auto socksAuth = static_cast<SocksAuthMode>(m_socksAuthCombo->currentData().toInt());

    auto setRowVisible = [this](QWidget *field, bool visible) {
        field->setVisible(visible);
        if (auto *label = qobject_cast<QLabel *>(m_form->labelForField(field))) {
            label->setVisible(visible);
        }
    };

    setRowVisible(m_localKindCombo, isLocal || isRemote);
    setRowVisible(m_localHostEdit, (isLocal || isRemote || isDynamic) && (isDynamic || localTcp));
    setRowVisible(m_localPortSpin, (isLocal || isRemote || isDynamic) && (isDynamic || localTcp));
    setRowVisible(m_localSocketRow, (isLocal || isRemote) && !localTcp);

    // Remote type: remote listen is always TCP; hide remote kind combo.
    setRowVisible(m_remoteKindCombo, isLocal);
    setRowVisible(m_remoteHostEdit, isLocal ? remoteTcp : isRemote);
    setRowVisible(m_remotePortSpin, isLocal ? remoteTcp : isRemote);
    setRowVisible(m_remoteSocketRow, isLocal && !remoteTcp);

    setRowVisible(m_socksAuthCombo, isDynamic);
    setRowVisible(m_socksUserEdit, isDynamic && socksAuth == SocksAuthMode::UsernamePassword);
    setRowVisible(m_socksPasswordEdit, isDynamic && socksAuth == SocksAuthMode::UsernamePassword);

    if (isRemote) {
        // Force remote kind UI to TCP semantics even if combo retained a value.
        setEndpointKindCombo(m_remoteKindCombo, TunnelEndpointKind::Tcp);
    }

    updateFieldLabels();
}

void TunnelDialog::accept()
{
    if (!validate()) {
        return;
    }
    QDialog::accept();
}

bool TunnelDialog::validate()
{
    const TunnelDefinition def = tunnel();
    const QString error = def.validationError();
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Tunnel"), error);
        return false;
    }
    if (def.type == TunnelType::Dynamic && def.socksAuth == SocksAuthMode::UsernamePassword) {
        if (m_mode == Mode::Create && socksPassword().isEmpty()) {
            QMessageBox::warning(this, tr("Tunnel"), tr("SOCKS password is required."));
            m_socksPasswordEdit->setFocus();
            return false;
        }
    }
    if ((def.type == TunnelType::Local || def.type == TunnelType::Remote) &&
        def.localKind == TunnelEndpointKind::UnixSocket && !localUnixSocketSupported()) {
        QMessageBox::warning(
            this, tr("Tunnel"), tr("Local Unix sockets are not supported on this platform."));
        return false;
    }
    return true;
}
