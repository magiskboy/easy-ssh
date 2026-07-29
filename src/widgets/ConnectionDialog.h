#pragma once

#include "Connection.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QPushButton;
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
    void onUseGatewayToggled(bool enabled);
    void onHopSelectionChanged();
    void onAddHop();
    void onRemoveHop();
    void browsePrivateKey();
    void browseGatewayPrivateKey();
    void accept() override;

private:
    void setupUi();
    bool validate();
    void updateAuthFieldsVisibility();
    void updateGatewayAuthFieldsVisibility();
    void updateGatewayPanelVisibility();
    void syncHopEditorFromCurrent();
    void syncCurrentHopFromEditor();
    void refreshHopList();
    int currentHopIndex() const;

    Mode m_mode;
    QUuid m_id;
    QList<JumpHop> m_jumpHops;

    QFormLayout *m_targetForm = nullptr;
    QGroupBox *m_gatewayGroup = nullptr;
    QGroupBox *m_advancedGroup = nullptr;

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

    QCheckBox *m_useGatewayCheck = nullptr;
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
    QFormLayout *m_gatewayForm = nullptr;
    QPushButton *m_addHopButton = nullptr;
    QPushButton *m_removeHopButton = nullptr;

    QSpinBox *m_keepAliveIntervalSpin = nullptr;
    QSpinBox *m_keepAliveCountSpin = nullptr;
    QCheckBox *m_compressionCheck = nullptr;
};
