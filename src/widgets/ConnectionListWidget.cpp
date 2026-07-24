#include "ConnectionListWidget.h"

#include "ConnectionDialog.h"
#include "ConnectionFilterProxy.h"
#include "ConnectionModel.h"
#include "ErrorNotifier.h"
#include "SecretStore.h"

#include <QAbstractItemView>
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
                   dialog.passphraseProvided());

    emit statusMessage(tr("Created connection: %1").arg(connection.name));
}

void ConnectionListWidget::editSelectedConnection()
{
    if (!m_model) {
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

    const AuthType previousAuthType = existing->authType;

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
                   dialog.passphraseProvided());

    emit statusMessage(tr("Updated connection: %1").arg(connection.name));
}

void ConnectionListWidget::deleteSelectedConnection()
{
    if (!m_model) {
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

    emit statusMessage(tr("Deleted connection: %1 (open sessions kept)").arg(name));
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
    if (!m_model) {
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
    }

    emit statusMessage(tr("Duplicated connection: %1").arg(copy->name));
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
        ->setEnabled(hasSelection);
    menu.addAction(tr("Duplicate"), this, &ConnectionListWidget::duplicateSelectedConnection)
        ->setEnabled(hasSelection);
    menu.addSeparator();
    menu.addAction(tr("Delete"), this, &ConnectionListWidget::deleteSelectedConnection)
        ->setEnabled(hasSelection);

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

void ConnectionListWidget::persistSecrets(const Connection &connection,
                                          AuthType previousAuthType,
                                          bool isEdit,
                                          const QString &password,
                                          bool passwordProvided,
                                          const QString &passphrase,
                                          bool passphraseProvided)
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
}

void ConnectionListWidget::warnSecretFailure(const QString &error)
{
    ErrorNotifier::notify(this,
                          tr("Keychain"),
                          tr("Could not store secret in the system keychain.\n%1").arg(error),
                          ErrorNotifier::Level::Warning);
}
