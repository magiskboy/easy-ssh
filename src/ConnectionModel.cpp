#include "ConnectionModel.h"

#include "ConnectionStore.h"
#include "TunnelStore.h"

ConnectionModel::ConnectionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConnectionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_connections.size();
}

QVariant ConnectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_connections.size()) {
        return {};
    }

    const Connection &connection = m_connections.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return connection.displayText();
    case IdRole:
        return connection.id;
    case NameRole:
        return connection.name;
    case HostRole:
        return connection.host;
    case PortRole:
        return connection.port;
    case UsernameRole:
        return connection.username;
    case AuthTypeRole:
        return static_cast<int>(connection.authType);
    case PrivateKeyPathRole:
        return connection.privateKeyPath;
    case StartupDirectoryRole:
        return connection.startupDirectory;
    default:
        return {};
    }
}

QHash<int, QByteArray> ConnectionModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {IdRole, QByteArrayLiteral("id")},
        {NameRole, QByteArrayLiteral("name")},
        {HostRole, QByteArrayLiteral("host")},
        {PortRole, QByteArrayLiteral("port")},
        {UsernameRole, QByteArrayLiteral("username")},
        {AuthTypeRole, QByteArrayLiteral("authType")},
        {PrivateKeyPathRole, QByteArrayLiteral("privateKeyPath")},
        {StartupDirectoryRole, QByteArrayLiteral("startupDirectory")},
    };
}

void ConnectionModel::loadAll()
{
    beginResetModel();
    m_connections = ConnectionStore::load();
    endResetModel();
}

bool ConnectionModel::add(const Connection &connection)
{
    if (connection.id.isNull() || connection.name.isEmpty()) {
        return false;
    }
    if (rowOf(connection.id) >= 0) {
        return false;
    }

    const int row = m_connections.size();
    beginInsertRows(QModelIndex(), row, row);
    m_connections.append(connection);
    endInsertRows();
    persist();
    return true;
}

bool ConnectionModel::update(const Connection &connection)
{
    const int row = rowOf(connection.id);
    if (row < 0) {
        return false;
    }

    m_connections[row] = connection;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
    persist();
    return true;
}

bool ConnectionModel::removeById(const QUuid &id)
{
    const int row = rowOf(id);
    if (row < 0) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_connections.removeAt(row);
    endRemoveRows();
    TunnelStore::removeByConnectionId(id);
    persist();
    return true;
}

std::optional<Connection> ConnectionModel::duplicate(const QUuid &id)
{
    const auto source = connectionById(id);
    if (!source) {
        return std::nullopt;
    }

    Connection copy = *source;
    copy.id = QUuid::createUuid();
    copy.name = QStringLiteral("%1 (copy)").arg(source->name);

    if (!add(copy)) {
        return std::nullopt;
    }
    return copy;
}

std::optional<Connection> ConnectionModel::connectionAt(int row) const
{
    if (row < 0 || row >= m_connections.size()) {
        return std::nullopt;
    }
    return m_connections.at(row);
}

std::optional<Connection> ConnectionModel::connectionById(const QUuid &id) const
{
    const int row = rowOf(id);
    if (row < 0) {
        return std::nullopt;
    }
    return m_connections.at(row);
}

int ConnectionModel::rowOf(const QUuid &id) const
{
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void ConnectionModel::persist()
{
    ConnectionStore::save(m_connections);
}
