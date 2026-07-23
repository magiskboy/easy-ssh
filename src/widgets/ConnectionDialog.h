#pragma once

#include "Connection.h"

#include <QDialog>

class QComboBox;
class QFormLayout;
class QLineEdit;
class QSpinBox;
class QWidget;

class ConnectionDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
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

private slots:
    void onAuthTypeChanged(int index);
    void browsePrivateKey();
    void accept() override;

private:
    void setupUi();
    bool validate();
    void updateAuthFieldsVisibility();

    Mode m_mode;
    QUuid m_id;
    QFormLayout *m_form = nullptr;

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
};
