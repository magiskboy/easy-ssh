#pragma once

#include "Tunnel.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QUuid>

#include <optional>

class TunnelListModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        TypeColumn,
        LocalColumn,
        RemoteColumn,
        StatusColumn,
        ColumnCount,
    };

    enum Roles {
        IdRole = Qt::UserRole + 1,
        EnabledRole,
        StatusDetailRole,
    };

    explicit TunnelListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTunnels(const QList<TunnelDefinition> &tunnels);
    void upsert(const TunnelDefinition &tunnel);
    bool removeById(const QUuid &id);
    void clear();

    void setRuntimeStatus(const QUuid &id, const QString &status, const QString &detail);
    void clearRuntimeStatuses();

    std::optional<TunnelDefinition> tunnelAt(int row) const;
    std::optional<TunnelDefinition> tunnelById(const QUuid &id) const;
    int rowOf(const QUuid &id) const;

    const QList<TunnelDefinition> &tunnels() const { return m_tunnels; }

private:
    struct RuntimeStatus {
        QString status = QStringLiteral("Off");
        QString detail;
    };

    QList<TunnelDefinition> m_tunnels;
    QHash<QUuid, RuntimeStatus> m_runtime;
};
