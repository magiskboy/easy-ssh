/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/tunnel/Tunnel.h"

#include <QDialog>
#include <QUuid>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QWidget;
class SecretStore;

class TunnelDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        Create,
        Edit,
    };

    explicit TunnelDialog(Mode mode, const QUuid &connectionId, QWidget *parent = nullptr);

    void setSecretStore(SecretStore *secretStore);

    void setTunnel(const TunnelDefinition &tunnel);
    TunnelDefinition tunnel() const;
    QString socksPassword() const;
    bool socksPasswordChanged() const { return m_socksPasswordChanged; }

private slots:
    void onTypeChanged(int index);
    void onLocalKindChanged(int index);
    void onRemoteKindChanged(int index);
    void onSocksAuthChanged(int index);
    void browseLocalSocketPath();
    void browseRemoteSocketPath();
    void accept() override;

private:
    void setupUi();
    bool validate();
    void updateFieldVisibility();
    void updateFieldLabels();
    void setEndpointKindCombo(QComboBox *combo, TunnelEndpointKind kind);
    TunnelEndpointKind endpointKindFromCombo(const QComboBox *combo) const;
    bool localUnixSocketSupported() const;
    QWidget *makeSocketPathRow(QLineEdit *edit, QPushButton *browseButton);
    /// Opens a path picker seeded from @p edit; returns empty if cancelled.
    QString chooseUnixSocketPath(QLineEdit *edit, const QString &caption);

    Mode m_mode;
    QUuid m_id;
    QUuid m_connectionId;
    SecretStore *m_secretStore = nullptr;

    QFormLayout *m_form = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;

    QComboBox *m_localKindCombo = nullptr;
    QLineEdit *m_localHostEdit = nullptr;
    QSpinBox *m_localPortSpin = nullptr;
    QWidget *m_localSocketRow = nullptr;
    QLineEdit *m_localSocketEdit = nullptr;
    QPushButton *m_localSocketBrowse = nullptr;

    QComboBox *m_remoteKindCombo = nullptr;
    QLineEdit *m_remoteHostEdit = nullptr;
    QSpinBox *m_remotePortSpin = nullptr;
    QWidget *m_remoteSocketRow = nullptr;
    QLineEdit *m_remoteSocketEdit = nullptr;
    QPushButton *m_remoteSocketBrowse = nullptr;

    QComboBox *m_socksAuthCombo = nullptr;
    QLineEdit *m_socksUserEdit = nullptr;
    QLineEdit *m_socksPasswordEdit = nullptr;

    QCheckBox *m_enabledCheck = nullptr;
    QLabel *m_hintLabel = nullptr;

    bool m_socksPasswordChanged = false;
    bool m_loadingSocksPassword = false;
};
