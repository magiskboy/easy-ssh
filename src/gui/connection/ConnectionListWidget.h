#pragma once

#include "core/connection/Connection.h"
#include "gui/ErrorNotifier.h"

#include <QUuid>
#include <QWidget>

#include <optional>

class ConnectionFilterProxy;
class ConnectionModel;
class QLineEdit;
class QListView;
class SecretStore;

class ConnectionListWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionListWidget(QWidget *parent = nullptr);

    void setConnectionModel(ConnectionModel *model);
    void setSecretStore(SecretStore *secretStore);

    void createConnection();
    void editSelectedConnection();
    void deleteSelectedConnection();
    void duplicateSelectedConnection();
    void importSelectedFromSshConfig();
    void reloadSshConfig();
    void openSelectedConnection();
    void focusSearch();

signals:
    void connectionActivated(const QUuid &id);
    void connectionSelected(const QUuid &id);
    void statusMessage(const QString &message, ErrorNotifier::Level level);

private slots:
    void onFilterTextChanged(const QString &text);
    void onActivated(const QModelIndex &index);
    void onSelectionChanged();
    void onContextMenu(const QPoint &pos);

private:
    std::optional<QUuid> selectedConnectionId() const;
    bool selectedIsAppConnection() const;
    bool selectedIsSshConfigConnection() const;
    void persistSecrets(const Connection &connection,
                        AuthType previousAuthType,
                        bool isEdit,
                        const QString &password,
                        bool passwordProvided,
                        const QString &passphrase,
                        bool passphraseProvided,
                        const QString &gatewayPassword,
                        bool gatewayPasswordProvided,
                        const QString &gatewayPassphrase,
                        bool gatewayPassphraseProvided);
    void warnSecretFailure(const QString &error);

    ConnectionModel *m_model = nullptr;
    SecretStore *m_secretStore = nullptr;
    ConnectionFilterProxy *m_proxy = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QListView *m_listView = nullptr;
};
