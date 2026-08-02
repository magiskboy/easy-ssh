/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"
#include "gui/ErrorNotifier.h"

#include <QDialog>
#include <QUuid>

#include <optional>

class ConnectionEditor;
class ConnectionFilterProxy;
class ConnectionModel;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QPushButton;
class SecretStore;

class ConnectionManagerDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionManagerDialog(QWidget *parent = nullptr);

    void setConnectionModel(ConnectionModel *model);
    void setSecretStore(SecretStore *secretStore);
    void selectConnection(const QUuid &id);

signals:
    void connectionActivated(const QUuid &id);
    void connectionEdited(const QUuid &id,
                          bool connectivityChanged,
                          bool targetSecretUpdated,
                          const QString &targetSecret,
                          bool gatewaySecretUpdated,
                          const QString &gatewaySecret);
    void statusMessage(const QString &message, ErrorNotifier::Level level);

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private slots:
    void onFilterTextChanged(const QString &text);
    void onSourceFilterChanged(int index);
    void onSelectionChanged();
    void onListActivated(const QModelIndex &index);
    void onNew();
    void onSave();
    void onDiscard();
    void onDelete();
    void onDuplicate();
    void onImportSelected();
    void onImportPrompt();
    void onReload();
    void onOpenSession();
    void onDirtyChanged(bool dirty);
    void onContextMenu(const QPoint &pos);

private:
    enum class PanelMode
    {
        Empty,
        Existing,
        Draft,
    };

    std::optional<QUuid> currentSelectedId() const;
    QList<QUuid> selectedIds() const;
    bool ensureCanLeaveSelection();
    bool promptSaveDiscardCancel();
    void loadSelection(const QUuid &id);
    void showEmptyState();
    void updateActionButtons();
    void selectIdInList(const QUuid &id);
    void emitEditSideEffects(const Connection &before,
                             const Connection &after,
                             const ConnectionEditor *editor);

    ConnectionModel *m_model = nullptr;
    SecretStore *m_secretStore = nullptr;
    ConnectionFilterProxy *m_proxy = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_sourceFilterCombo = nullptr;
    QListView *m_listView = nullptr;
    QLabel *m_emptyLabel = nullptr;
    ConnectionEditor *m_editor = nullptr;
    QWidget *m_detailPane = nullptr;

    QPushButton *m_newButton = nullptr;
    QPushButton *m_importButton = nullptr;
    QPushButton *m_reloadButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_discardButton = nullptr;
    QPushButton *m_duplicateButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_importSelectedButton = nullptr;

    PanelMode m_panelMode = PanelMode::Empty;
    std::optional<Connection> m_loadedConnection;
    bool m_blockSelectionHandler = false;
};
