#pragma once

#include <QSortFilterProxyModel>

class ConnectionFilterProxy final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ConnectionFilterProxy(QObject *parent = nullptr);

    void setFilterText(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
};
