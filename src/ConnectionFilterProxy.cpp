#include "ConnectionFilterProxy.h"

#include "ConnectionModel.h"

#include <QtGlobal>

ConnectionFilterProxy::ConnectionFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void ConnectionFilterProxy::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_filterText = text;
    endFilterChange(Direction::Rows);
#else
    // begin/endFilterChange require Qt 6.9 / 6.10; CI and many distros are still on 6.6–6.8.
    m_filterText = text;
    invalidateFilter();
#endif
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
