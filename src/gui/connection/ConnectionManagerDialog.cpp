// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionManagerDialog.h"

#include "ConnectionEditor.h"
#include "ConnectionSecretHelper.h"
#include "core/connection/SecretStore.h"
#include "core/tunnel/Tunnel.h"
#include "core/tunnel/TunnelStore.h"
#include "gui/ErrorNotifier.h"
#include "gui/dialogs/ModelessDialog.h"
#include "gui/models/ConnectionFilterProxy.h"
#include "gui/models/ConnectionModel.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

ConnectionManagerDialog::ConnectionManagerDialog(QWidget *parent) : QDialog(parent)
{
    configureModelessDialog(this);

    setWindowTitle(tr("Connection Manager"));
    resize(960, 640);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search…"));
    m_searchEdit->setClearButtonEnabled(true);

    m_sourceFilterCombo = new QComboBox(this);
    m_sourceFilterCombo->addItem(tr("All"), -1);
    m_sourceFilterCombo->addItem(tr("Easy SSH"), static_cast<int>(ConnectionSource::App));
    m_sourceFilterCombo->addItem(tr("SSH Config"), static_cast<int>(ConnectionSource::SshConfig));

    m_newButton = new QPushButton(tr("New"), this);
    m_importButton = new QPushButton(tr("Import…"), this);
    m_reloadButton = new QPushButton(tr("Reload"), this);

    auto *toolbar = new QHBoxLayout();
    toolbar->addWidget(m_searchEdit, 1);
    toolbar->addWidget(new QLabel(tr("Source:"), this));
    toolbar->addWidget(m_sourceFilterCombo);
    toolbar->addWidget(m_newButton);
    toolbar->addWidget(m_importButton);
    toolbar->addWidget(m_reloadButton);

    m_listView = new QListView(this);
    m_listView->setUniformItemSizes(true);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_proxy = new ConnectionFilterProxy(this);

    m_emptyLabel = new QLabel(tr("Select a connection"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    m_editor = new ConnectionEditor(this);
    m_editor->setVisible(false);

    m_openButton = new QPushButton(tr("Open Session"), this);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_discardButton = new QPushButton(tr("Discard"), this);
    m_duplicateButton = new QPushButton(tr("Duplicate"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_importSelectedButton = new QPushButton(tr("Import to Easy SSH…"), this);

    auto *actionRow = new QHBoxLayout();
    actionRow->addWidget(m_openButton);
    actionRow->addWidget(m_saveButton);
    actionRow->addWidget(m_discardButton);
    actionRow->addWidget(m_duplicateButton);
    actionRow->addWidget(m_deleteButton);
    actionRow->addWidget(m_importSelectedButton);
    actionRow->addStretch(1);

    m_detailPane = new QWidget(this);
    auto *detailLayout = new QVBoxLayout(m_detailPane);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->addWidget(m_emptyLabel, 1);
    detailLayout->addWidget(m_editor, 1);
    detailLayout->addLayout(actionRow);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_listView);
    splitter->addWidget(m_detailPane);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({280, 680});

    auto *closeButton = new QPushButton(tr("Close"), this);
    auto *footer = new QHBoxLayout();
    footer->addStretch(1);
    footer->addWidget(closeButton);

    auto *root = new QVBoxLayout(this);
    root->addLayout(toolbar);
    root->addWidget(splitter, 1);
    root->addLayout(footer);

    connect(
        m_searchEdit, &QLineEdit::textChanged, this, &ConnectionManagerDialog::onFilterTextChanged);
    connect(m_sourceFilterCombo,
            &QComboBox::currentIndexChanged,
            this,
            &ConnectionManagerDialog::onSourceFilterChanged);
    connect(m_newButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onNew);
    connect(m_importButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onImportPrompt);
    connect(m_reloadButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onReload);
    connect(m_openButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onOpenSession);
    connect(m_saveButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onSave);
    connect(m_discardButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onDiscard);
    connect(m_duplicateButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onDuplicate);
    connect(m_deleteButton, &QPushButton::clicked, this, &ConnectionManagerDialog::onDelete);
    connect(m_importSelectedButton,
            &QPushButton::clicked,
            this,
            &ConnectionManagerDialog::onImportSelected);
    connect(closeButton, &QPushButton::clicked, this, &ConnectionManagerDialog::close);
    connect(m_listView, &QListView::activated, this, &ConnectionManagerDialog::onListActivated);
    connect(m_listView,
            &QListView::customContextMenuRequested,
            this,
            &ConnectionManagerDialog::onContextMenu);
    connect(
        m_editor, &ConnectionEditor::dirtyChanged, this, &ConnectionManagerDialog::onDirtyChanged);

    showEmptyState();
    updateActionButtons();
}

void ConnectionManagerDialog::setConnectionModel(ConnectionModel *model)
{
    m_model = model;
    m_proxy->setSourceModel(m_model);
    m_listView->setModel(m_proxy);

    if (m_listView->selectionModel()) {
        connect(m_listView->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                &ConnectionManagerDialog::onSelectionChanged);
    }
}

void ConnectionManagerDialog::setSecretStore(SecretStore *secretStore)
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
                    ErrorNotifier::notify(
                        this,
                        tr("Keychain"),
                        tr("Could not store secret in the system keychain.\n%1").arg(error),
                        ErrorNotifier::Level::Warning);
                }
            });
}

void ConnectionManagerDialog::selectConnection(const QUuid &id)
{
    if (id.isNull()) {
        return;
    }
    selectIdInList(id);
}

void ConnectionManagerDialog::closeEvent(QCloseEvent *event)
{
    if (!ensureCanLeaveSelection()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void ConnectionManagerDialog::reject()
{
    if (!ensureCanLeaveSelection()) {
        return;
    }
    QDialog::reject();
}

void ConnectionManagerDialog::onFilterTextChanged(const QString &text)
{
    m_proxy->setFilterText(text);
}

void ConnectionManagerDialog::onSourceFilterChanged(int index)
{
    Q_UNUSED(index);
    const int value = m_sourceFilterCombo->currentData().toInt();
    if (value < 0) {
        m_proxy->setSourceFilter(std::nullopt);
    } else {
        m_proxy->setSourceFilter(static_cast<ConnectionSource>(value));
    }
}

void ConnectionManagerDialog::onSelectionChanged()
{
    if (m_blockSelectionHandler) {
        return;
    }

    const auto ids = selectedIds();
    if (ids.size() != 1) {
        if (!ensureCanLeaveSelection()) {
            // Restore previous single selection if possible.
            if (m_loadedConnection) {
                m_blockSelectionHandler = true;
                selectIdInList(m_loadedConnection->id);
                m_blockSelectionHandler = false;
            }
            return;
        }
        if (m_panelMode == PanelMode::Draft) {
            // Keep draft visible when multi-select / clear happens only after leave check.
        }
        if (ids.isEmpty()) {
            if (m_panelMode != PanelMode::Draft) {
                showEmptyState();
            }
            updateActionButtons();
            return;
        }
        // Multi-select: keep editor if single was loaded, but disable edit actions via buttons.
        updateActionButtons();
        return;
    }

    const QUuid id = ids.first();
    if (m_panelMode == PanelMode::Existing && m_loadedConnection && m_loadedConnection->id == id) {
        updateActionButtons();
        return;
    }
    if (m_panelMode == PanelMode::Draft && m_editor->connection().id == id) {
        updateActionButtons();
        return;
    }

    if (!ensureCanLeaveSelection()) {
        if (m_loadedConnection) {
            m_blockSelectionHandler = true;
            selectIdInList(m_loadedConnection->id);
            m_blockSelectionHandler = false;
        } else if (m_panelMode == PanelMode::Draft) {
            m_blockSelectionHandler = true;
            m_listView->selectionModel()->clearSelection();
            m_blockSelectionHandler = false;
        }
        return;
    }

    loadSelection(id);
}

void ConnectionManagerDialog::onListActivated(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    const QUuid id = index.data(ConnectionModel::IdRole).toUuid();
    if (!id.isNull()) {
        emit connectionActivated(id);
    }
}

void ConnectionManagerDialog::onNew()
{
    if (!ensureCanLeaveSelection()) {
        return;
    }

    m_blockSelectionHandler = true;
    m_listView->selectionModel()->clearSelection();
    m_blockSelectionHandler = false;

    m_panelMode = PanelMode::Draft;
    m_loadedConnection.reset();
    m_emptyLabel->setVisible(false);
    m_editor->setVisible(true);
    m_editor->setReadOnly(false);
    m_editor->setMode(ConnectionEditor::Mode::Create);
    m_editor->clear();
    updateActionButtons();
}

void ConnectionManagerDialog::onSave()
{
    if (!m_model || !m_editor->isVisible() || m_editor->isReadOnly()) {
        return;
    }
    if (!m_editor->validate()) {
        return;
    }

    Connection connection = m_editor->connection();
    connection.source = ConnectionSource::App;
    connection.configAlias.clear();

    if (m_panelMode == PanelMode::Draft) {
        if (!m_model->add(connection)) {
            ErrorNotifier::notify(this,
                                  tr("Error"),
                                  tr("Failed to create connection."),
                                  ErrorNotifier::Level::Warning);
            return;
        }
        ConnectionSecretHelper::persistSecrets(m_secretStore,
                                               connection,
                                               AuthType::Password,
                                               false,
                                               m_editor->password(),
                                               m_editor->passwordProvided(),
                                               m_editor->passphrase(),
                                               m_editor->passphraseProvided(),
                                               m_editor->gatewayPassword(),
                                               m_editor->gatewayPasswordProvided(),
                                               m_editor->gatewayPassphrase(),
                                               m_editor->gatewayPassphraseProvided());
        m_editor->markClean();
        emit statusMessage(tr("Created connection: %1").arg(connection.name),
                           ErrorNotifier::Level::Success);
        selectIdInList(connection.id);
        loadSelection(connection.id);
        return;
    }

    if (!m_loadedConnection || m_loadedConnection->source != ConnectionSource::App) {
        return;
    }

    const Connection before = *m_loadedConnection;
    const AuthType previousAuthType = before.authType;
    if (!m_model->update(connection)) {
        ErrorNotifier::notify(
            this, tr("Error"), tr("Failed to update connection."), ErrorNotifier::Level::Warning);
        return;
    }

    ConnectionSecretHelper::persistSecrets(m_secretStore,
                                           connection,
                                           previousAuthType,
                                           true,
                                           m_editor->password(),
                                           m_editor->passwordProvided(),
                                           m_editor->passphrase(),
                                           m_editor->passphraseProvided(),
                                           m_editor->gatewayPassword(),
                                           m_editor->gatewayPasswordProvided(),
                                           m_editor->gatewayPassphrase(),
                                           m_editor->gatewayPassphraseProvided());

    m_editor->markClean();
    m_loadedConnection = connection;
    emit statusMessage(tr("Updated connection: %1").arg(connection.name),
                       ErrorNotifier::Level::Success);
    emitEditSideEffects(before, connection, m_editor);
    updateActionButtons();
}

void ConnectionManagerDialog::onDiscard()
{
    if (m_panelMode == PanelMode::Draft) {
        showEmptyState();
        updateActionButtons();
        return;
    }
    if (!m_loadedConnection) {
        return;
    }
    loadSelection(m_loadedConnection->id);
}

void ConnectionManagerDialog::onDelete()
{
    if (!m_model) {
        return;
    }

    QList<QUuid> ids = selectedIds();
    if (ids.isEmpty() && m_loadedConnection &&
        m_loadedConnection->source == ConnectionSource::App) {
        ids.append(m_loadedConnection->id);
    }

    QList<Connection> toDelete;
    for (const QUuid &id : ids) {
        const auto connection = m_model->connectionById(id);
        if (connection && connection->source == ConnectionSource::App) {
            toDelete.append(*connection);
        }
    }
    if (toDelete.isEmpty()) {
        return;
    }

    const QString message = toDelete.size() == 1
                                ? tr("Delete connection \"%1\"?").arg(toDelete.first().name)
                                : tr("Delete %1 connections?").arg(toDelete.size());
    const auto answer = QMessageBox::question(this,
                                              tr("Delete Connection"),
                                              message,
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    // Allow leaving without save prompt — deletion supersedes dirty state.
    m_editor->markClean();
    m_panelMode = PanelMode::Empty;
    m_loadedConnection.reset();

    for (const Connection &connection : toDelete) {
        const QList<TunnelDefinition> tunnels = TunnelStore::loadForConnection(connection.id);
        if (!m_model->removeById(connection.id)) {
            ErrorNotifier::notify(this,
                                  tr("Error"),
                                  tr("Failed to delete connection."),
                                  ErrorNotifier::Level::Warning);
            continue;
        }
        if (m_secretStore) {
            m_secretStore->deleteAllSecrets(connection.id);
            for (const TunnelDefinition &tunnel : tunnels) {
                m_secretStore->deleteSecret(tunnel.id, SecretStore::Kind::TunnelSocksPassword);
            }
        }
        emit statusMessage(tr("Deleted connection: %1 (open sessions kept)").arg(connection.name),
                           ErrorNotifier::Level::Warning);
    }

    showEmptyState();
    updateActionButtons();
}

void ConnectionManagerDialog::onDuplicate()
{
    if (!m_model) {
        return;
    }
    if (!ensureCanLeaveSelection()) {
        return;
    }

    const auto id = currentSelectedId();
    if (!id) {
        return;
    }
    const auto existing = m_model->connectionById(*id);
    if (!existing || existing->source != ConnectionSource::App) {
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
    selectIdInList(copy->id);
    loadSelection(copy->id);
}

void ConnectionManagerDialog::onImportSelected()
{
    if (!m_model) {
        return;
    }
    if (!ensureCanLeaveSelection()) {
        return;
    }

    QList<QUuid> ids = selectedIds();
    if (ids.isEmpty() && m_loadedConnection &&
        m_loadedConnection->source == ConnectionSource::SshConfig) {
        ids.append(m_loadedConnection->id);
    }

    QList<QUuid> configIds;
    for (const QUuid &id : ids) {
        const auto connection = m_model->connectionById(id);
        if (connection && connection->source == ConnectionSource::SshConfig) {
            configIds.append(id);
        }
    }
    if (configIds.isEmpty()) {
        return;
    }

    if (configIds.size() > 1) {
        const auto answer =
            QMessageBox::question(this,
                                  tr("Import from SSH Config"),
                                  tr("Import %1 hosts into Easy SSH?").arg(configIds.size()),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::Yes);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    QUuid lastImported;
    for (const QUuid &id : configIds) {
        const auto imported = m_model->importFromSshConfig(id);
        if (!imported) {
            ErrorNotifier::notify(this,
                                  tr("Error"),
                                  tr("Failed to import SSH config host."),
                                  ErrorNotifier::Level::Warning);
            continue;
        }
        lastImported = imported->id;
        emit statusMessage(tr("Imported connection: %1").arg(imported->name),
                           ErrorNotifier::Level::Success);
    }

    if (!lastImported.isNull()) {
        selectIdInList(lastImported);
        loadSelection(lastImported);
    }
}

void ConnectionManagerDialog::onImportPrompt()
{
    if (!m_model) {
        return;
    }
    if (!ensureCanLeaveSelection()) {
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
    selectIdInList(imported->id);
    loadSelection(imported->id);
}

void ConnectionManagerDialog::onReload()
{
    if (!m_model) {
        return;
    }
    if (!ensureCanLeaveSelection()) {
        return;
    }
    const auto previous = currentSelectedId();
    m_model->reloadSshConfig();
    emit statusMessage(tr("Reloaded ~/.ssh/config"), ErrorNotifier::Level::Success);
    if (previous && m_model->connectionById(*previous)) {
        selectIdInList(*previous);
        loadSelection(*previous);
    } else {
        showEmptyState();
    }
}

void ConnectionManagerDialog::onOpenSession()
{
    const auto id = currentSelectedId();
    if (id) {
        emit connectionActivated(*id);
        return;
    }
    if (m_loadedConnection) {
        emit connectionActivated(m_loadedConnection->id);
    }
}

void ConnectionManagerDialog::onDirtyChanged(bool dirty)
{
    Q_UNUSED(dirty);
    updateActionButtons();
}

void ConnectionManagerDialog::onContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_listView->indexAt(pos);
    if (index.isValid()) {
        m_listView->setCurrentIndex(index);
    }

    const auto ids = selectedIds();
    bool anyApp = false;
    bool anyConfig = false;
    for (const QUuid &id : ids) {
        const auto connection = m_model ? m_model->connectionById(id) : std::nullopt;
        if (!connection) {
            continue;
        }
        if (connection->source == ConnectionSource::App) {
            anyApp = true;
        } else {
            anyConfig = true;
        }
    }

    QMenu menu(this);
    menu.addAction(tr("Open Session"), this, &ConnectionManagerDialog::onOpenSession)
        ->setEnabled(ids.size() == 1);
    menu.addSeparator();
    menu.addAction(tr("New"), this, &ConnectionManagerDialog::onNew);
    menu.addAction(tr("Duplicate"), this, &ConnectionManagerDialog::onDuplicate)
        ->setEnabled(ids.size() == 1 && anyApp);
    menu.addAction(tr("Import to Easy SSH…"), this, &ConnectionManagerDialog::onImportSelected)
        ->setEnabled(anyConfig);
    menu.addSeparator();
    menu.addAction(tr("Delete"), this, &ConnectionManagerDialog::onDelete)->setEnabled(anyApp);
    menu.exec(m_listView->viewport()->mapToGlobal(pos));
}

std::optional<QUuid> ConnectionManagerDialog::currentSelectedId() const
{
    const auto ids = selectedIds();
    if (ids.size() != 1) {
        return std::nullopt;
    }
    return ids.first();
}

QList<QUuid> ConnectionManagerDialog::selectedIds() const
{
    QList<QUuid> ids;
    if (!m_listView->selectionModel()) {
        return ids;
    }
    const QModelIndexList indexes = m_listView->selectionModel()->selectedIndexes();
    for (const QModelIndex &index : indexes) {
        const QUuid id = index.data(ConnectionModel::IdRole).toUuid();
        if (!id.isNull() && !ids.contains(id)) {
            ids.append(id);
        }
    }
    return ids;
}

bool ConnectionManagerDialog::ensureCanLeaveSelection()
{
    if (!m_editor->isDirty()) {
        return true;
    }
    return promptSaveDiscardCancel();
}

bool ConnectionManagerDialog::promptSaveDiscardCancel()
{
    const auto answer =
        QMessageBox::question(this,
                              tr("Unsaved Changes"),
                              tr("The connection has unsaved changes."),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                              QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    if (answer == QMessageBox::Save) {
        onSave();
        return !m_editor->isDirty();
    }
    // Discard
    if (m_panelMode == PanelMode::Draft) {
        m_editor->markClean();
        return true;
    }
    if (m_loadedConnection) {
        const QUuid id = m_loadedConnection->id;
        m_editor->markClean();
        loadSelection(id);
    } else {
        m_editor->markClean();
    }
    return true;
}

void ConnectionManagerDialog::loadSelection(const QUuid &id)
{
    if (!m_model) {
        showEmptyState();
        return;
    }
    const auto connection = m_model->connectionById(id);
    if (!connection) {
        showEmptyState();
        return;
    }

    m_panelMode = PanelMode::Existing;
    m_loadedConnection = connection;
    m_emptyLabel->setVisible(false);
    m_editor->setVisible(true);
    const bool isApp = connection->source == ConnectionSource::App;
    m_editor->setReadOnly(!isApp);
    m_editor->setMode(isApp ? ConnectionEditor::Mode::Edit : ConnectionEditor::Mode::Create);
    m_editor->setConnection(*connection);
    updateActionButtons();
}

void ConnectionManagerDialog::showEmptyState()
{
    m_panelMode = PanelMode::Empty;
    m_loadedConnection.reset();
    m_editor->markClean();
    m_editor->setVisible(false);
    m_emptyLabel->setVisible(true);
    updateActionButtons();
}

void ConnectionManagerDialog::updateActionButtons()
{
    const auto ids = selectedIds();
    const bool single = ids.size() == 1;
    const bool dirty = m_editor->isDirty();
    const bool editing = m_editor->isVisible() &&
                         (m_panelMode == PanelMode::Existing || m_panelMode == PanelMode::Draft);
    const bool isApp = m_loadedConnection && m_loadedConnection->source == ConnectionSource::App;
    const bool isConfig =
        m_loadedConnection && m_loadedConnection->source == ConnectionSource::SshConfig;
    const bool draft = m_panelMode == PanelMode::Draft;

    bool anyApp = false;
    bool anyConfig = false;
    for (const QUuid &id : ids) {
        const auto connection = m_model ? m_model->connectionById(id) : std::nullopt;
        if (!connection) {
            continue;
        }
        if (connection->source == ConnectionSource::App) {
            anyApp = true;
        } else {
            anyConfig = true;
        }
    }

    m_openButton->setEnabled(single && !draft);
    m_saveButton->setEnabled(editing && !m_editor->isReadOnly() && (dirty || draft));
    m_discardButton->setEnabled(editing && dirty);
    m_duplicateButton->setEnabled(single && isApp && !draft);
    m_deleteButton->setEnabled(anyApp || (draft == false && isApp && ids.isEmpty()));
    m_importSelectedButton->setEnabled(anyConfig || (isConfig && ids.isEmpty()));
}

void ConnectionManagerDialog::selectIdInList(const QUuid &id)
{
    if (!m_model || !m_proxy) {
        return;
    }
    const int sourceRow = m_model->rowOf(id);
    if (sourceRow < 0) {
        return;
    }
    const QModelIndex sourceIndex = m_model->index(sourceRow);
    const QModelIndex proxyIndex = m_proxy->mapFromSource(sourceIndex);
    if (!proxyIndex.isValid()) {
        return;
    }
    m_blockSelectionHandler = true;
    m_listView->setCurrentIndex(proxyIndex);
    m_listView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect);
    m_blockSelectionHandler = false;
}

void ConnectionManagerDialog::emitEditSideEffects(const Connection &before,
                                                  const Connection &after,
                                                  const ConnectionEditor *editor)
{
    const bool secretsTouched = editor->passwordProvided() || editor->passphraseProvided() ||
                                editor->gatewayPasswordProvided() ||
                                editor->gatewayPassphraseProvided() ||
                                before.savePassword != after.savePassword;
    const bool connectivityChanged =
        before.host != after.host || before.port != after.port ||
        before.username != after.username || before.authType != after.authType ||
        before.privateKeyPath != after.privateKeyPath || before.proxyMode != after.proxyMode ||
        before.usesJumpHost() != after.usesJumpHost() ||
        before.proxyCommand != after.proxyCommand ||
        before.jumpHops.size() != after.jumpHops.size() ||
        before.agentForwarding != after.agentForwarding || secretsTouched;

    bool targetSecretUpdated = false;
    QString targetSecret;
    if (after.authType == AuthType::Password && editor->passwordProvided()) {
        targetSecretUpdated = true;
        targetSecret = editor->password();
    } else if (after.authType == AuthType::PrivateKey && editor->passphraseProvided()) {
        targetSecretUpdated = true;
        targetSecret = editor->passphrase();
    }

    bool gatewaySecretUpdated = false;
    QString gatewaySecret;
    const bool usesCustomGateway =
        after.usesJumpHost() && !after.jumpHops.first().useTargetCredentials;
    if (usesCustomGateway) {
        const AuthType gatewayAuth = after.jumpHops.first().authType;
        if (gatewayAuth == AuthType::Password && editor->gatewayPasswordProvided()) {
            gatewaySecretUpdated = true;
            gatewaySecret = editor->gatewayPassword();
        } else if (gatewayAuth == AuthType::PrivateKey && editor->gatewayPassphraseProvided()) {
            gatewaySecretUpdated = true;
            gatewaySecret = editor->gatewayPassphrase();
        }
    }

    emit connectionEdited(after.id,
                          connectivityChanged,
                          targetSecretUpdated,
                          targetSecret,
                          gatewaySecretUpdated,
                          gatewaySecret);
}
