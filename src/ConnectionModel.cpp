#include "ConnectionModel.h"

#include "ConnectionStore.h"
#include "SshConfigParser.h"
#include "TunnelStore.h"

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

ConnectionModel::ConnectionModel(QObject *parent) : QAbstractListModel(parent) {}

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
    case SourceRole:
        return static_cast<int>(connection.source);
    case ConfigAliasRole:
        return connection.configAlias;
    case UsesJumpHostRole:
        return connection.usesJumpHost();
    case KeepAliveIntervalRole:
        return connection.keepAliveIntervalSec;
    case CompressionEnabledRole:
        return connection.compressionEnabled;
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
        {SourceRole, QByteArrayLiteral("source")},
        {ConfigAliasRole, QByteArrayLiteral("configAlias")},
        {UsesJumpHostRole, QByteArrayLiteral("usesJumpHost")},
        {KeepAliveIntervalRole, QByteArrayLiteral("keepAliveIntervalSec")},
        {CompressionEnabledRole, QByteArrayLiteral("compressionEnabled")},
    };
}

void ConnectionModel::loadAll()
{
    rebuildMergedList(ConnectionStore::load());
    ensureConfigWatcher();
}

void ConnectionModel::reloadSshConfig()
{
    rebuildMergedList(appConnectionsOnly());
    ensureConfigWatcher();
}

bool ConnectionModel::add(const Connection &connection)
{
    if (connection.id.isNull() || connection.name.isEmpty()) {
        return false;
    }
    if (connection.source != ConnectionSource::App) {
        return false;
    }
    if (rowOf(connection.id) >= 0) {
        return false;
    }

    // Insert before ssh-config overlay rows so app connections stay grouped at the top.
    int insertRow = 0;
    while (insertRow < m_connections.size() &&
           m_connections.at(insertRow).source == ConnectionSource::App) {
        ++insertRow;
    }

    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_connections.insert(insertRow, connection);
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
    if (m_connections.at(row).source != ConnectionSource::App ||
        connection.source != ConnectionSource::App) {
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
    if (m_connections.at(row).source != ConnectionSource::App) {
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
    if (!source || source->source != ConnectionSource::App) {
        return std::nullopt;
    }

    Connection copy = *source;
    copy.id = QUuid::createUuid();
    copy.name = QStringLiteral("%1 (copy)").arg(source->name);
    copy.source = ConnectionSource::App;
    copy.configAlias.clear();

    if (!add(copy)) {
        return std::nullopt;
    }
    return copy;
}

std::optional<Connection> ConnectionModel::importFromSshConfig(const QUuid &id)
{
    const auto source = connectionById(id);
    if (!source || source->source != ConnectionSource::SshConfig) {
        return std::nullopt;
    }

    Connection copy = *source;
    copy.id = QUuid::createUuid();
    copy.source = ConnectionSource::App;
    copy.configAlias.clear();

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
    ConnectionStore::save(appConnectionsOnly());
}

void ConnectionModel::rebuildMergedList(const QList<Connection> &appConnections)
{
    beginResetModel();
    m_connections = appConnections;
    for (Connection &connection : m_connections) {
        connection.source = ConnectionSource::App;
        connection.configAlias.clear();
    }
    m_connections.append(loadSshConfigConnections());
    endResetModel();
}

void ConnectionModel::ensureConfigWatcher()
{
    if (m_configWatcher == nullptr) {
        m_configWatcher = new QFileSystemWatcher(this);
        connect(
            m_configWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
                // Editors often replace the file atomically; re-watch after change.
                if (QFileInfo::exists(path) && !m_configWatcher->files().contains(path)) {
                    m_configWatcher->addPath(path);
                }
                if (m_reloadDebounce == nullptr) {
                    m_reloadDebounce = new QTimer(this);
                    m_reloadDebounce->setSingleShot(true);
                    m_reloadDebounce->setInterval(300);
                    connect(m_reloadDebounce,
                            &QTimer::timeout,
                            this,
                            &ConnectionModel::reloadSshConfig);
                }
                m_reloadDebounce->start();
            });
    }

    const QString path = SshConfigParser::defaultConfigPath();
    if (!QFileInfo::exists(path)) {
        return;
    }
    if (!m_configWatcher->files().contains(path)) {
        m_configWatcher->addPath(path);
    }
}

QList<Connection> ConnectionModel::appConnectionsOnly() const
{
    QList<Connection> app;
    for (const Connection &connection : m_connections) {
        if (connection.source == ConnectionSource::App) {
            app.append(connection);
        }
    }
    return app;
}

QList<Connection> ConnectionModel::loadSshConfigConnections() const
{
    return SshConfigParser::toConnections(SshConfigParser::load());
}
