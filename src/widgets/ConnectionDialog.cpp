#include "ConnectionDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_id(QUuid::createUuid())
{
    setupUi();
    setWindowTitle(mode == Mode::Create ? tr("New Connection") : tr("Edit Connection"));
    resize(460, 320);
}

void ConnectionDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    m_form = new QFormLayout();

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

    m_form->addRow(tr("Name"), m_nameEdit);
    m_form->addRow(tr("Host"), m_hostEdit);
    m_form->addRow(tr("Port"), m_portSpin);
    m_form->addRow(tr("Username"), m_usernameEdit);
    m_form->addRow(tr("Authentication"), m_authTypeCombo);
    m_form->addRow(tr("Password"), m_passwordEdit);
    m_form->addRow(tr("Private Key"), m_privateKeyRow);
    m_form->addRow(tr("Passphrase"), m_passphraseEdit);
    m_form->addRow(tr("Startup Directory"), m_startupDirEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addLayout(m_form);
    layout->addWidget(buttons);

    connect(m_authTypeCombo,
            &QComboBox::currentIndexChanged,
            this,
            &ConnectionDialog::onAuthTypeChanged);
    connect(browseButton, &QPushButton::clicked, this, &ConnectionDialog::browsePrivateKey);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateAuthFieldsVisibility();
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
    updateAuthFieldsVisibility();
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

void ConnectionDialog::onAuthTypeChanged(int index)
{
    Q_UNUSED(index);
    updateAuthFieldsVisibility();
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

void ConnectionDialog::accept()
{
    if (!validate()) {
        return;
    }
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

    return true;
}

void ConnectionDialog::updateAuthFieldsVisibility()
{
    const auto authType = static_cast<AuthType>(m_authTypeCombo->currentData().toInt());
    const bool usePassword = authType == AuthType::Password;

    if (!m_form) {
        return;
    }

    for (int i = 0; i < m_form->rowCount(); ++i) {
        QLayoutItem *fieldItem = m_form->itemAt(i, QFormLayout::FieldRole);
        if (!fieldItem || !fieldItem->widget()) {
            continue;
        }
        QWidget *field = fieldItem->widget();

        if (field == m_passwordEdit) {
            m_form->setRowVisible(i, usePassword);
        } else if (field == m_privateKeyRow || field == m_passphraseEdit) {
            m_form->setRowVisible(i, !usePassword);
        }
    }
}
