/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/fs/SftpTypes.h"

#include <QAbstractItemModel>
#include <QString>
#include <QVector>
#include <memory>
#include <vector>

class Session;

class RemoteFileModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles
    {
        PathRole = Qt::UserRole + 1,
        IsDirRole,
    };

    explicit RemoteFileModel(QObject *parent = nullptr);
    ~RemoteFileModel() override;

    void bindSession(Session *session);
    void unbindSession();
    Session *session() const;

    void setRootPath(const QString &path);
    QString rootPath() const;
    void refresh(const QModelIndex &index = {});
    void refreshPath(const QString &path);
    void setShowHiddenFiles(bool show);
    bool showHiddenFiles() const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant
    headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &parent = {}) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    QString pathForIndex(const QModelIndex &index) const;
    bool isDirectory(const QModelIndex &index) const;
    bool isParentNavEntry(const QModelIndex &index) const;
    QModelIndex indexForPath(const QString &path) const;
    QString parentDirectory(const QModelIndex &index) const;
    QString currentDirectory(const QModelIndex &index) const;
    bool hasChildNamed(const QString &name) const;
    static QString parentPathOf(const QString &path);

private:
    struct Node
    {
        RemoteEntry entry;
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        bool fetched = false;
        bool fetching = false;
        bool populated = false;
    };

    Node *nodeFromIndex(const QModelIndex &index) const;
    QModelIndex indexForNode(Node *node, int column = 0) const;
    void requestList(Node *node);
    void applyListing(const QString &path, const QVector<RemoteEntry> &entries);
    Node *findNodeByPath(Node *node, const QString &path) const;
    static QString formatSize(qint64 size);
    static QString formatMtime(qint64 mtime);

    Session *m_session = nullptr;
    std::unique_ptr<Node> m_root;
    QString m_rootPath;
    bool m_showHiddenFiles = false;
};
