#include "ConnectionFilterProxy.h"

#include "ConnectionModel.h"

ConnectionFilterProxy::ConnectionFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void ConnectionFilterProxy::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    beginFilterChange();
    m_filterText = text;
    endFilterChange();
}

bool ConnectionFilterProxy::filterAcceptsRow(int sourceRow,
                                             const QModelIndex &sourceParent) const
{
    const QString needle = m_filterText.trimmed();
    if (needle.isEmpty()) {
        return true;
    }

    const QAbstractItemModel *model = sourceModel();
    if (!model) {
        return false;
    }

    const QModelIndex index = model->index(sourceRow, 0, sourceParent);
    const QString name = model->data(index, ConnectionModel::NameRole).toString();
    const QString host = model->data(index, ConnectionModel::HostRole).toString();
    const QString username = model->data(index, ConnectionModel::UsernameRole).toString();

    return name.contains(needle, Qt::CaseInsensitive)
        || host.contains(needle, Qt::CaseInsensitive)
        || username.contains(needle, Qt::CaseInsensitive);
}
