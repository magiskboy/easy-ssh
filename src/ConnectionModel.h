#pragma once

#include "Connection.h"

#include <QAbstractListModel>
#include <QList>
#include <optional>

class ConnectionModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        HostRole,
        PortRole,
        UsernameRole,
        AuthTypeRole,
        PrivateKeyPathRole,
        StartupDirectoryRole,
    };

    explicit ConnectionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void loadAll();
    bool add(const Connection &connection);
    bool update(const Connection &connection);
    bool removeById(const QUuid &id);
    std::optional<Connection> duplicate(const QUuid &id);

    std::optional<Connection> connectionAt(int row) const;
    std::optional<Connection> connectionById(const QUuid &id) const;
    int rowOf(const QUuid &id) const;

    const QList<Connection> &connections() const { return m_connections; }

private:
    void persist();

    QList<Connection> m_connections;
};
