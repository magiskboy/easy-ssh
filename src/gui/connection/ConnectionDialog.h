/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"

#include <QDialog>
#include <QList>

class CategoryDialogShell;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QWidget;

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

private slots:
    void onAuthTypeChanged(int index);
    void onGatewayAuthTypeChanged(int index);
    void onProxyModeButtonClicked(int id);
    void onHopSelectionChanged();
    void onAddHop();
    void onRemoveHop();
    void browsePrivateKey();
    void browseGatewayPrivateKey();
    void resetShellCommandsToDefaults();
    void accept() override;

private:
    void setupUi();
    QWidget *createSessionPage();
    QWidget *createConnectionPage();
    QWidget *createProxyPage();
    QWidget *createScpShellPage();
    bool validate();
    void updateAuthFieldsVisibility();
    void updateGatewayAuthFieldsVisibility();
    void updateProxyPanelVisibility();
    void setProxyMode(SshProxyMode mode, bool confirmClear);
    bool proxyJumpHasData() const;
    bool proxyCommandHasData() const;
    void syncHopEditorFromCurrent();
    void syncCurrentHopFromEditor();
    void refreshHopList();
    int currentHopIndex() const;
    void applyShellCommandsToForm(const ShellCommandSetConfig &config);
    ShellCommandSetConfig shellCommandsFromForm() const;

    Mode m_mode;
    QUuid m_id;
    QList<JumpHop> m_jumpHops;
    SshProxyMode m_proxyMode = SshProxyMode::None;

    CategoryDialogShell *m_shell = nullptr;
    QFormLayout *m_authForm = nullptr;
    QFormLayout *m_gatewayForm = nullptr;

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QComboBox *m_authTypeCombo = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_privateKeyEdit = nullptr;
    QWidget *m_privateKeyRow = nullptr;
    QLineEdit *m_passphraseEdit = nullptr;
    QLineEdit *m_startupDirEdit = nullptr;

    QButtonGroup *m_proxyModeGroup = nullptr;
    QRadioButton *m_proxyNoneRadio = nullptr;
    QRadioButton *m_proxyJumpRadio = nullptr;
    QRadioButton *m_proxyCommandRadio = nullptr;
    QGroupBox *m_jumpPanel = nullptr;
    QGroupBox *m_commandPanel = nullptr;
    QLineEdit *m_proxyCommandEdit = nullptr;
    QListWidget *m_hopList = nullptr;
    QLineEdit *m_gatewayHostEdit = nullptr;
    QSpinBox *m_gatewayPortSpin = nullptr;
    QLineEdit *m_gatewayUsernameEdit = nullptr;
    QCheckBox *m_useTargetCredentialsCheck = nullptr;
    QComboBox *m_gatewayAuthTypeCombo = nullptr;
    QLineEdit *m_gatewayPasswordEdit = nullptr;
    QLineEdit *m_gatewayPrivateKeyEdit = nullptr;
    QWidget *m_gatewayPrivateKeyRow = nullptr;
    QLineEdit *m_gatewayPassphraseEdit = nullptr;
    QPushButton *m_addHopButton = nullptr;
    QPushButton *m_removeHopButton = nullptr;

    QSpinBox *m_keepAliveIntervalSpin = nullptr;
    QSpinBox *m_keepAliveCountSpin = nullptr;
    QCheckBox *m_compressionCheck = nullptr;

    QCheckBox *m_allowScpFallbackCheck = nullptr;
    QLineEdit *m_shellEdit = nullptr;
    QLineEdit *m_listingCommandEdit = nullptr;
    QCheckBox *m_clearAliasesCheck = nullptr;
    QCheckBox *m_clearNationalVarsCheck = nullptr;
    QCheckBox *m_tryFullTimeCheck = nullptr;
    QCheckBox *m_ignoreLsWarningsCheck = nullptr;
    QLineEdit *m_mkdirCommandEdit = nullptr;
    QLineEdit *m_removeCommandEdit = nullptr;
    QLineEdit *m_renameCommandEdit = nullptr;
    QLineEdit *m_realpathCommandEdit = nullptr;
    QPushButton *m_resetShellCommandsButton = nullptr;
};
