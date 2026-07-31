/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/session/Session.h"
#include "core/tunnel/Tunnel.h"
#include "gui/ErrorNotifier.h"

#include <QWidget>

#include <optional>

class QAction;
class QLabel;
class QTableView;
class SecretStore;
class TunnelListModel;

class TunnelListWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TunnelListWidget(QWidget *parent = nullptr);

    void setSecretStore(SecretStore *secretStore);
    void bindSession(Session *session);
    void unbindSession();

signals:
    void statusMessage(const QString &message, ErrorNotifier::Level level);

private slots:
    void addTunnel();
    void editSelected();
    void deleteSelected();
    void toggleSelected();
    void onSelectionChanged();
    void onCustomContextMenu(const QPoint &pos);
    void onTunnelStatusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void onTunnelError(const QUuid &tunnelId, const QString &message);
    void onSessionStateChanged(SessionState state);

private:
    void reloadFromStore();
    void persistAll();
    void updateActionsEnabled();
    void showEmptyState(const QString &message);
    void showList();
    void updateSessionBadge();
    std::optional<TunnelDefinition> selectedTunnel() const;
    bool isSessionConnected() const;
    void persistSocksPassword(const TunnelDefinition &def, const QString &password, bool changed);
    void deleteSocksPassword(const QUuid &tunnelId);
    void startTunnelWithSecrets(TunnelDefinition def);
    void startEnabledAuthTunnels();

    TunnelListModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QWidget *m_listHost = nullptr;

    QAction *m_addAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_toggleAction = nullptr;

    Session *m_session = nullptr;
    SecretStore *m_secretStore = nullptr;
};
