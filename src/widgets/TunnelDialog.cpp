#include "TunnelDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

TunnelDialog::TunnelDialog(Mode mode, const QUuid &connectionId, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_id(QUuid::createUuid()), m_connectionId(connectionId)
{
    setupUi();
    setWindowTitle(mode == Mode::Create ? tr("New Tunnel") : tr("Edit Tunnel"));
    resize(460, 340);
}

void TunnelDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    m_form = new QFormLayout();

    m_nameEdit = new QLineEdit(this);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Local Port Forward"), static_cast<int>(TunnelType::Local));
    m_typeCombo->addItem(tr("Remote Port Forward"), static_cast<int>(TunnelType::Remote));

    m_localHostEdit = new QLineEdit(this);
    m_localHostEdit->setText(QStringLiteral("127.0.0.1"));
    m_localPortSpin = new QSpinBox(this);
    m_localPortSpin->setRange(1, 65535);
    m_localPortSpin->setValue(3306);

    m_remoteHostEdit = new QLineEdit(this);
    m_remoteHostEdit->setText(QStringLiteral("127.0.0.1"));
    m_remotePortSpin = new QSpinBox(this);
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(3306);

    m_enabledCheck = new QCheckBox(tr("Enable when session connects"), this);
    m_enabledCheck->setChecked(true);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));

    m_form->addRow(tr("Name"), m_nameEdit);
    m_form->addRow(tr("Type"), m_typeCombo);
    m_form->addRow(tr("Local Host"), m_localHostEdit);
    m_form->addRow(tr("Local Port"), m_localPortSpin);
    m_form->addRow(tr("Remote Host"), m_remoteHostEdit);
    m_form->addRow(tr("Remote Port"), m_remotePortSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addLayout(m_form);
    layout->addWidget(m_enabledCheck);
    layout->addWidget(m_hintLabel);
    layout->addWidget(buttons);

    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, &TunnelDialog::onTypeChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &TunnelDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateFieldLabels();
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

    m_localHostEdit->setText(tunnel.localHost);
    m_localPortSpin->setValue(tunnel.localPort);
    m_remoteHostEdit->setText(tunnel.remoteHost);
    m_remotePortSpin->setValue(tunnel.remotePort);
    m_enabledCheck->setChecked(tunnel.enabled);
    updateFieldLabels();
}

TunnelDefinition TunnelDialog::tunnel() const
{
    TunnelDefinition def;
    def.id = m_id;
    def.connectionId = m_connectionId;
    def.name = m_nameEdit->text().trimmed();
    def.type = static_cast<TunnelType>(m_typeCombo->currentData().toInt());
    def.localHost = m_localHostEdit->text().trimmed();
    def.localPort = static_cast<quint16>(m_localPortSpin->value());
    def.remoteHost = m_remoteHostEdit->text().trimmed();
    def.remotePort = static_cast<quint16>(m_remotePortSpin->value());
    def.enabled = m_enabledCheck->isChecked();
    return def;
}

void TunnelDialog::onTypeChanged(int)
{
    updateFieldLabels();
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
        setLabel(m_localHostEdit, tr("Local Host (destination)"));
        setLabel(m_localPortSpin, tr("Local Port (destination)"));
        setLabel(m_remoteHostEdit, tr("Remote Listen Host"));
        setLabel(m_remotePortSpin, tr("Remote Listen Port"));
        m_hintLabel->setText(
            tr("SSH server listens remotely and forwards connections to this machine. "
               "Binding beyond 127.0.0.1 on the server may require GatewayPorts."));
    } else {
        setLabel(m_localHostEdit, tr("Local Bind Host"));
        setLabel(m_localPortSpin, tr("Local Bind Port"));
        setLabel(m_remoteHostEdit, tr("Remote Host (destination)"));
        setLabel(m_remotePortSpin, tr("Remote Port (destination)"));
        m_hintLabel->setText(
            tr("Connect your client to the local bind address; traffic goes through SSH "
               "to the remote endpoint."));
    }
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
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Tunnel"), tr("Name is required."));
        m_nameEdit->setFocus();
        return false;
    }
    if (m_localHostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Tunnel"), tr("Local host is required."));
        m_localHostEdit->setFocus();
        return false;
    }
    if (m_remoteHostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Tunnel"), tr("Remote host is required."));
        m_remoteHostEdit->setFocus();
        return false;
    }
    if (m_connectionId.isNull()) {
        QMessageBox::warning(this, tr("Tunnel"), tr("No connection selected."));
        return false;
    }
    return true;
}
