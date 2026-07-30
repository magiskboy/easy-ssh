#pragma once

#include "core/tunnel/Tunnel.h"

#include <QDialog>
#include <QUuid>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QSpinBox;

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

    void setTunnel(const TunnelDefinition &tunnel);
    TunnelDefinition tunnel() const;

private slots:
    void onTypeChanged(int index);
    void accept() override;

private:
    void setupUi();
    bool validate();
    void updateFieldLabels();

    Mode m_mode;
    QUuid m_id;
    QUuid m_connectionId;

    QFormLayout *m_form = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_localHostEdit = nullptr;
    QSpinBox *m_localPortSpin = nullptr;
    QLineEdit *m_remoteHostEdit = nullptr;
    QSpinBox *m_remotePortSpin = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QLabel *m_hintLabel = nullptr;
};
