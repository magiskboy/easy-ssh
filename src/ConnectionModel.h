#pragma once

#include "Connection.h"

#include <QAbstractListModel>
#include <QList>
#include <optional>

class QFileSystemWatcher;
class QTimer;

class ConnectionModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        HostRole,
        PortRole,
        UsernameRole,
        AuthTypeRole,
        PrivateKeyPathRole,
        StartupDirectoryRole,
        SourceRole,
        ConfigAliasRole,
    };

    explicit ConnectionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void loadAll();
    void reloadSshConfig();
    bool add(const Connection &connection);
    bool update(const Connection &connection);
    bool removeById(const QUuid &id);
    std::optional<Connection> duplicate(const QUuid &id);
    std::optional<Connection> importFromSshConfig(const QUuid &id);

    std::optional<Connection> connectionAt(int row) const;
    std::optional<Connection> connectionById(const QUuid &id) const;
    int rowOf(const QUuid &id) const;

    const QList<Connection> &connections() const { return m_connections; }

private:
    void persist();
    void rebuildMergedList(const QList<Connection> &appConnections);
    void ensureConfigWatcher();
    QList<Connection> appConnectionsOnly() const;
    QList<Connection> loadSshConfigConnections() const;

    QList<Connection> m_connections;
    QFileSystemWatcher *m_configWatcher = nullptr;
    QTimer *m_reloadDebounce = nullptr;
};
