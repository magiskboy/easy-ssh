/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ConnectionEditor.h"

#include <QDialog>

class ConnectionDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        Create,
        Edit,
    };

    explicit ConnectionDialog(Mode mode, QWidget *parent = nullptr);

    void setConnection(const Connection &connection);
    Connection connection() const;

    QString password() const;
    QString passphrase() const;
    bool passwordProvided() const;
    bool passphraseProvided() const;

    QString gatewayPassword() const;
    QString gatewayPassphrase() const;
    bool gatewayPasswordProvided() const;
    bool gatewayPassphraseProvided() const;

private:
    void accept() override;

    ConnectionEditor *m_editor = nullptr;
};
