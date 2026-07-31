// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionListWidget.h"

#include "ConnectionDialog.h"
#include "core/connection/SecretStore.h"
#include "gui/ErrorNotifier.h"
#include "gui/models/ConnectionFilterProxy.h"
#include "gui/models/ConnectionModel.h"

#include <QAbstractItemView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>

ConnectionListWidget::ConnectionListWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search…"));
    m_searchEdit->setClearButtonEnabled(true);

    m_listView = new QListView(this);
    m_listView->setUniformItemSizes(true);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);

    m_proxy = new ConnectionFilterProxy(this);

    layout->addWidget(m_searchEdit);
    layout->addWidget(m_listView, 1);

    connect(
        m_searchEdit, &QLineEdit::textChanged, this, &ConnectionListWidget::onFilterTextChanged);
    connect(m_listView, &QListView::activated, this, &ConnectionListWidget::onActivated);
    connect(m_listView,
            &QListView::customContextMenuRequested,
            this,
            &ConnectionListWidget::onContextMenu);
}

void ConnectionListWidget::setConnectionModel(ConnectionModel *model)
{
    m_model = model;
    m_proxy->setSourceModel(m_model);
    m_listView->setModel(m_proxy);

    if (m_listView->selectionModel()) {
        connect(m_listView->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                &ConnectionListWidget::onSelectionChanged);
    }
}

void ConnectionListWidget::setSecretStore(SecretStore *secretStore)
{
    m_secretStore = secretStore;
    if (!m_secretStore) {
        return;
    }

    connect(m_secretStore,
            &SecretStore::storeFinished,
            this,
            [this](const QUuid &, SecretStore::Kind, bool ok, const QString &error) {
                if (!ok) {
                    warnSecretFailure(error);
                }
            });
}

void ConnectionListWidget::createConnection()
{
    if (!m_model) {
        return;
    }

    ConnectionDialog dialog(ConnectionDialog::Mode::Create, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const Connection connection = dialog.connection();
    if (!m_model->add(connection)) {
        ErrorNotifier::notify(
            this, tr("Error"), tr("Failed to create connection."), ErrorNotifier::Level::Warning);
        return;
    }

    persistSecrets(connection,
                   AuthType::Password,
                   false,
                   dialog.password(),
                   dialog.passwordProvided(),
                   dialog.passphrase(),
                   dialog.passphraseProvided(),
                   dialog.gatewayPassword(),
                   dialog.gatewayPasswordProvided(),
                   dialog.gatewayPassphrase(),
                   dialog.gatewayPassphraseProvided());

    emit statusMessage(tr("Created connection: %1").arg(connection.name),
                       ErrorNotifier::Level::Success);
}

void ConnectionListWidget::editSelectedConnection()
{
    const auto id = selectedConnectionId();
    if (!id) {
        return;
    }
    editConnectionById(*id);
}

void ConnectionListWidget::editConnectionById(const QUuid &id)
{
    if (!m_model || id.isNull()) {
        return;
    }

    const auto existing = m_model->connectionById(id);
    if (!existing || existing->source != ConnectionSource::App) {
        ErrorNotifier::notify(this,
                              tr("Edit Connection"),
                              tr("Only Easy SSH connections can be edited."),
                              ErrorNotifier::Level::Warning);
        return;
    }

    const AuthType previousAuthType = existing->authType;
    const Connection before = *existing;

    ConnectionDialog dialog(ConnectionDialog::Mode::Edit, this);
    dialog.setConnection(*existing);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const Connection connection = dialog.connection();
    if (!m_model->update(connection)) {
        ErrorNotifier::notify(
            this, tr("Error"), tr("Failed to update connection."), ErrorNotifier::Level::Warning);
        return;
    }

    persistSecrets(connection,
                   previousAuthType,
                   true,
                   dialog.password(),
                   dialog.passwordProvided(),
                   dialog.passphrase(),
                   dialog.passphraseProvided(),
                   dialog.gatewayPassword(),
                   dialog.gatewayPasswordProvided(),
                   dialog.gatewayPassphrase(),
                   dialog.gatewayPassphraseProvided());

    emit statusMessage(tr("Updated connection: %1").arg(connection.name),
                       ErrorNotifier::Level::Success);

    const bool connectivityChanged =
        before.host != connection.host || before.port != connection.port ||
        before.username != connection.username || before.authType != connection.authType ||
        before.privateKeyPath != connection.privateKeyPath ||
        before.usesJumpHost() != connection.usesJumpHost();
    emit connectionEdited(connection.id, connectivityChanged);
}

void ConnectionListWidget::deleteSelectedConnection()
{
    if (!m_model || !selectedIsAppConnection()) {
        return;
    }

    const auto id = selectedConnectionId();
    if (!id) {
        return;
    }

    const auto existing = m_model->connectionById(*id);
    if (!existing) {
        return;
    }

    const auto answer = QMessageBox::question(this,
                                              tr("Delete Connection"),
                                              tr("Delete connection \"%1\"?").arg(existing->name),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const QString name = existing->name;
    if (!m_model->removeById(*id)) {
        ErrorNotifier::notify(
            this, tr("Error"), tr("Failed to delete connection."), ErrorNotifier::Level::Warning);
        return;
    }

    if (m_secretStore) {
        m_secretStore->deleteAllSecrets(*id);
    }

    emit statusMessage(tr("Deleted connection: %1 (open sessions kept)").arg(name),
                       ErrorNotifier::Level::Warning);
}

void ConnectionListWidget::openSelectedConnection()
{
    const auto id = selectedConnectionId();
    if (!id) {
        return;
    }
    emit connectionActivated(*id);
}

void ConnectionListWidget::focusSearch()
{
    if (m_searchEdit) {
        m_searchEdit->setFocus(Qt::ShortcutFocusReason);
        m_searchEdit->selectAll();
    }
}

void ConnectionListWidget::duplicateSelectedConnection()
{
    if (!m_model || !selectedIsAppConnection()) {
        return;
    }

    const auto id = selectedConnectionId();
    if (!id) {
        return;
    }

    const auto copy = m_model->duplicate(*id);
    if (!copy) {
        ErrorNotifier::notify(this,
                              tr("Error"),
                              tr("Failed to duplicate connection."),
                              ErrorNotifier::Level::Warning);
        return;
    }

    if (m_secretStore) {
        m_secretStore->copySecret(*id, copy->id, SecretStore::Kind::Password);
        m_secretStore->copySecret(*id, copy->id, SecretStore::Kind::Passphrase);
        m_secretStore->copySecret(*id, copy->id, SecretStore::Kind::GatewayPassword);
        m_secretStore->copySecret(*id, copy->id, SecretStore::Kind::GatewayPassphrase);
    }

    emit statusMessage(tr("Duplicated connection: %1").arg(copy->name),
                       ErrorNotifier::Level::Success);
}

void ConnectionListWidget::importSelectedFromSshConfig()
{
    if (!m_model || !selectedIsSshConfigConnection()) {
        return;
    }

    const auto id = selectedConnectionId();
    if (!id) {
        return;
    }

    const auto imported = m_model->importFromSshConfig(*id);
    if (!imported) {
        ErrorNotifier::notify(this,
                              tr("Error"),
                              tr("Failed to import SSH config host."),
                              ErrorNotifier::Level::Warning);
        return;
    }

    emit statusMessage(tr("Imported connection: %1").arg(imported->name),
                       ErrorNotifier::Level::Success);
}

void ConnectionListWidget::promptImportFromSshConfig()
{
    if (!m_model) {
        return;
    }

    QStringList labels;
    QList<QUuid> ids;
    for (const Connection &connection : m_model->connections()) {
        if (connection.source != ConnectionSource::SshConfig) {
            continue;
        }
        labels.append(connection.displayText());
        ids.append(connection.id);
    }
    if (labels.isEmpty()) {
        ErrorNotifier::notify(this,
                              tr("Import"),
                              tr("No SSH config hosts found. Check ~/.ssh/config."),
                              ErrorNotifier::Level::Warning);
        return;
    }

    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, tr("Import from SSH Config"), tr("Host:"), labels, 0, false, &ok);
    if (!ok || chosen.isEmpty()) {
        return;
    }
    const int index = labels.indexOf(chosen);
    if (index < 0 || index >= ids.size()) {
        return;
    }

    const auto imported = m_model->importFromSshConfig(ids.at(index));
    if (!imported) {
        ErrorNotifier::notify(this,
                              tr("Error"),
                              tr("Failed to import SSH config host."),
                              ErrorNotifier::Level::Warning);
        return;
    }

    emit statusMessage(tr("Imported connection: %1").arg(imported->name),
                       ErrorNotifier::Level::Success);
}

void ConnectionListWidget::reloadSshConfig()
{
    if (!m_model) {
        return;
    }
    m_model->reloadSshConfig();
    emit statusMessage(tr("Reloaded ~/.ssh/config"), ErrorNotifier::Level::Success);
}

void ConnectionListWidget::onFilterTextChanged(const QString &text)
{
    m_proxy->setFilterText(text);
}

void ConnectionListWidget::onActivated(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    const QUuid id = index.data(ConnectionModel::IdRole).toUuid();
    if (!id.isNull()) {
        emit connectionActivated(id);
    }
}

void ConnectionListWidget::onSelectionChanged()
{
    const auto id = selectedConnectionId();
    if (id) {
        emit connectionSelected(*id);
    }
}

void ConnectionListWidget::onContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_listView->indexAt(pos);
    if (index.isValid()) {
        m_listView->setCurrentIndex(index);
    }

    const bool hasSelection = selectedConnectionId().has_value();
    const bool isApp = selectedIsAppConnection();
    const bool isConfig = selectedIsSshConfigConnection();

    QMenu menu(this);
    menu.addAction(tr("Open Session"),
                   this,
                   [this]() {
                       const auto id = selectedConnectionId();
                       if (id) {
                           emit connectionActivated(*id);
                       }
                   })
        ->setEnabled(hasSelection);
    menu.addSeparator();
    menu.addAction(tr("New Connection…"), this, &ConnectionListWidget::createConnection);
    menu.addAction(tr("Edit…"), this, &ConnectionListWidget::editSelectedConnection)
        ->setEnabled(isApp);
    menu.addAction(tr("Duplicate"), this, &ConnectionListWidget::duplicateSelectedConnection)
        ->setEnabled(isApp);
    menu.addAction(
            tr("Import to Easy SSH…"), this, &ConnectionListWidget::importSelectedFromSshConfig)
        ->setEnabled(isConfig);
    menu.addSeparator();
    menu.addAction(tr("Reload SSH Config"), this, &ConnectionListWidget::reloadSshConfig);
    menu.addSeparator();
    menu.addAction(tr("Delete"), this, &ConnectionListWidget::deleteSelectedConnection)
        ->setEnabled(isApp);

    menu.exec(m_listView->viewport()->mapToGlobal(pos));
}

std::optional<QUuid> ConnectionListWidget::selectedConnectionId() const
{
    const QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) {
        return std::nullopt;
    }
    const QUuid id = index.data(ConnectionModel::IdRole).toUuid();
    if (id.isNull()) {
        return std::nullopt;
    }
    return id;
}

bool ConnectionListWidget::selectedIsAppConnection() const
{
    const auto id = selectedConnectionId();
    if (!id || !m_model) {
        return false;
    }
    const auto connection = m_model->connectionById(*id);
    return connection && connection->source == ConnectionSource::App;
}

bool ConnectionListWidget::selectedIsSshConfigConnection() const
{
    const auto id = selectedConnectionId();
    if (!id || !m_model) {
        return false;
    }
    const auto connection = m_model->connectionById(*id);
    return connection && connection->source == ConnectionSource::SshConfig;
}

void ConnectionListWidget::persistSecrets(const Connection &connection,
                                          AuthType previousAuthType,
                                          bool isEdit,
                                          const QString &password,
                                          bool passwordProvided,
                                          const QString &passphrase,
                                          bool passphraseProvided,
                                          const QString &gatewayPassword,
                                          bool gatewayPasswordProvided,
                                          const QString &gatewayPassphrase,
                                          bool gatewayPassphraseProvided)
{
    if (!m_secretStore) {
        return;
    }

    if (isEdit && previousAuthType != connection.authType) {
        if (previousAuthType == AuthType::Password) {
            m_secretStore->deleteSecret(connection.id, SecretStore::Kind::Password);
        } else {
            m_secretStore->deleteSecret(connection.id, SecretStore::Kind::Passphrase);
        }
    }

    if (connection.authType == AuthType::Password) {
        if (passwordProvided) {
            m_secretStore->storeSecret(connection.id, SecretStore::Kind::Password, password);
        }
        if (isEdit) {
            m_secretStore->deleteSecret(connection.id, SecretStore::Kind::Passphrase);
        }
    } else {
        if (passphraseProvided) {
            m_secretStore->storeSecret(connection.id, SecretStore::Kind::Passphrase, passphrase);
        }
        if (isEdit) {
            m_secretStore->deleteSecret(connection.id, SecretStore::Kind::Password);
        }
    }

    const bool usesCustomGateway =
        connection.usesJumpHost() && !connection.jumpHops.first().useTargetCredentials;
    if (!usesCustomGateway) {
        m_secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassword);
        m_secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassphrase);
        return;
    }

    const AuthType gatewayAuth = connection.jumpHops.first().authType;
    if (gatewayAuth == AuthType::Password) {
        if (gatewayPasswordProvided) {
            m_secretStore->storeSecret(
                connection.id, SecretStore::Kind::GatewayPassword, gatewayPassword);
        }
        m_secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassphrase);
    } else {
        if (gatewayPassphraseProvided) {
            m_secretStore->storeSecret(
                connection.id, SecretStore::Kind::GatewayPassphrase, gatewayPassphrase);
        }
        m_secretStore->deleteSecret(connection.id, SecretStore::Kind::GatewayPassword);
    }
}

void ConnectionListWidget::warnSecretFailure(const QString &error)
{
    ErrorNotifier::notify(this,
                          tr("Keychain"),
                          tr("Could not store secret in the system keychain.\n%1").arg(error),
                          ErrorNotifier::Level::Warning);
}
