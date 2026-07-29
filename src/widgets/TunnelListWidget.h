#pragma once

#include "ErrorNotifier.h"
#include "Tunnel.h"
#include "widgets/TerminalSessionWidget.h"

#include <QWidget>

#include <optional>

class QAction;
class QLabel;
class QTableView;
class TunnelListModel;

class TunnelListWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TunnelListWidget(QWidget *parent = nullptr);

    void bindSession(TerminalSessionWidget *session);
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
    void onSessionStateChanged(TerminalSessionWidget::State state);

private:
    void reloadFromStore();
    void persistAll();
    void updateActionsEnabled();
    void showEmptyState(const QString &message);
    void showList();
    void updateSessionBadge();
    std::optional<TunnelDefinition> selectedTunnel() const;
    bool isSessionConnected() const;

    TunnelListModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_sessionBadge = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QWidget *m_listHost = nullptr;

    QAction *m_addAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_toggleAction = nullptr;

    TerminalSessionWidget *m_session = nullptr;
};
