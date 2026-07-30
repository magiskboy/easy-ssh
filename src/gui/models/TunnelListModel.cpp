#include "TunnelListModel.h"

TunnelListModel::TunnelListModel(QObject *parent) : QAbstractTableModel(parent) {}

int TunnelListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_tunnels.size();
}

int TunnelListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant TunnelListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tunnels.size()) {
        return {};
    }

    const TunnelDefinition &tunnel = m_tunnels.at(index.row());
    const RuntimeStatus runtime = m_runtime.value(tunnel.id);

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (index.column()) {
        case NameColumn:
            return tunnel.name;
        case TypeColumn:
            switch (tunnel.type) {
            case TunnelType::Remote:
                return QStringLiteral("Remote");
            case TunnelType::Dynamic:
                return QStringLiteral("Dynamic");
            case TunnelType::Local:
                return QStringLiteral("Local");
            }
            return QStringLiteral("Local");
        case LocalColumn:
            return tunnel.localAddress();
        case RemoteColumn:
            return tunnel.remoteAddress();
        case StatusColumn:
            if (role == Qt::ToolTipRole && !runtime.detail.isEmpty()) {
                return runtime.detail;
            }
            return runtime.status;
        default:
            return {};
        }
    }

    switch (role) {
    case IdRole:
        return tunnel.id;
    case EnabledRole:
        return tunnel.enabled;
    case StatusDetailRole:
        return runtime.detail;
    case Qt::UserRole:
        return QVariant::fromValue(tunnel);
    default:
        return {};
    }
}

QVariant TunnelListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case NameColumn:
        return tr("Name");
    case TypeColumn:
        return tr("Type");
    case LocalColumn:
        return tr("Local");
    case RemoteColumn:
        return tr("Remote");
    case StatusColumn:
        return tr("Status");
    default:
        return {};
    }
}

QHash<int, QByteArray> TunnelListModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {IdRole, QByteArrayLiteral("id")},
        {EnabledRole, QByteArrayLiteral("enabled")},
        {StatusDetailRole, QByteArrayLiteral("statusDetail")},
    };
}

void TunnelListModel::setTunnels(const QList<TunnelDefinition> &tunnels)
{
    const QHash<QUuid, RuntimeStatus> previous = m_runtime;
    beginResetModel();
    m_tunnels = tunnels;
    m_runtime.clear();
    for (const TunnelDefinition &tunnel : m_tunnels) {
        m_runtime.insert(tunnel.id, previous.value(tunnel.id));
    }
    endResetModel();
}

void TunnelListModel::upsert(const TunnelDefinition &tunnel)
{
    const int row = rowOf(tunnel.id);
    if (row < 0) {
        const int insertRow = m_tunnels.size();
        beginInsertRows(QModelIndex(), insertRow, insertRow);
        m_tunnels.append(tunnel);
        m_runtime.insert(tunnel.id, {});
        endInsertRows();
        return;
    }

    m_tunnels[row] = tunnel;
    const QModelIndex topLeft = index(row, 0);
    const QModelIndex bottomRight = index(row, ColumnCount - 1);
    emit dataChanged(topLeft, bottomRight);
}

bool TunnelListModel::removeById(const QUuid &id)
{
    const int row = rowOf(id);
    if (row < 0) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_tunnels.removeAt(row);
    m_runtime.remove(id);
    endRemoveRows();
    return true;
}

void TunnelListModel::clear()
{
    beginResetModel();
    m_tunnels.clear();
    m_runtime.clear();
    endResetModel();
}

void TunnelListModel::setRuntimeStatus(const QUuid &id,
                                       const QString &status,
                                       const QString &detail)
{
    const int row = rowOf(id);
    if (row < 0) {
        return;
    }

    m_runtime[id] = RuntimeStatus{status, detail};
    const QModelIndex idx = index(row, StatusColumn);
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::ToolTipRole, StatusDetailRole});
}

void TunnelListModel::clearRuntimeStatuses()
{
    if (m_tunnels.isEmpty()) {
        return;
    }

    for (auto it = m_runtime.begin(); it != m_runtime.end(); ++it) {
        it->status = QStringLiteral("Off");
        it->detail.clear();
    }

    const QModelIndex topLeft = index(0, StatusColumn);
    const QModelIndex bottomRight = index(m_tunnels.size() - 1, StatusColumn);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::ToolTipRole, StatusDetailRole});
}

std::optional<TunnelDefinition> TunnelListModel::tunnelAt(int row) const
{
    if (row < 0 || row >= m_tunnels.size()) {
        return std::nullopt;
    }
    return m_tunnels.at(row);
}

std::optional<TunnelDefinition> TunnelListModel::tunnelById(const QUuid &id) const
{
    const int row = rowOf(id);
    if (row < 0) {
        return std::nullopt;
    }
    return m_tunnels.at(row);
}

int TunnelListModel::rowOf(const QUuid &id) const
{
    for (int i = 0; i < m_tunnels.size(); ++i) {
        if (m_tunnels.at(i).id == id) {
            return i;
        }
    }
    return -1;
}
