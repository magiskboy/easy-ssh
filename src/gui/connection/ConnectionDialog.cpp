// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionDialog.h"

#include "gui/dialogs/ModelessDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(Mode mode, QWidget *parent) : QDialog(parent), m_mode(mode)
{
    configureModelessDialog(this);

    m_editor = new ConnectionEditor(this);
    m_editor->setMode(mode == Mode::Create ? ConnectionEditor::Mode::Create
                                           : ConnectionEditor::Mode::Edit);

    setWindowTitle(mode == Mode::Create ? tr("New Connection") : tr("Edit Connection"));
    resize(640, 520);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(QDialogButtonBox::Cancel);

    if (mode == Mode::Create) {
        auto *saveButton = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
        auto *saveAndConnectButton =
            buttons->addButton(tr("Save and Connect"), QDialogButtonBox::AcceptRole);
        saveAndConnectButton->setDefault(true);
        saveAndConnectButton->setAutoDefault(true);
        connect(saveButton, &QPushButton::clicked, this, [this]() { acceptSave(false); });
        connect(saveAndConnectButton, &QPushButton::clicked, this, [this]() { acceptSave(true); });
    } else {
        auto *saveButton = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
        saveButton->setDefault(true);
        connect(saveButton, &QPushButton::clicked, this, [this]() { acceptSave(false); });
    }

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_editor, 1);
    layout->addWidget(buttons);
}

void ConnectionDialog::setConnection(const Connection &connection)
{
    m_editor->setConnection(connection);
}

Connection ConnectionDialog::connection() const
{
    return m_editor->connection();
}

QString ConnectionDialog::password() const
{
    return m_editor->password();
}

QString ConnectionDialog::passphrase() const
{
    return m_editor->passphrase();
}

bool ConnectionDialog::passwordProvided() const
{
    return m_editor->passwordProvided();
}

bool ConnectionDialog::passphraseProvided() const
{
    return m_editor->passphraseProvided();
}

QString ConnectionDialog::gatewayPassword() const
{
    return m_editor->gatewayPassword();
}

QString ConnectionDialog::gatewayPassphrase() const
{
    return m_editor->gatewayPassphrase();
}

bool ConnectionDialog::gatewayPasswordProvided() const
{
    return m_editor->gatewayPasswordProvided();
}

bool ConnectionDialog::gatewayPassphraseProvided() const
{
    return m_editor->gatewayPassphraseProvided();
}

void ConnectionDialog::accept()
{
    acceptSave(m_mode == Mode::Create ? m_connectAfterAccept : false);
}

void ConnectionDialog::acceptSave(bool connectAfter)
{
    if (!m_editor->validate()) {
        return;
    }
    m_connectAfterAccept = connectAfter && m_mode == Mode::Create;
    QDialog::accept();
}
