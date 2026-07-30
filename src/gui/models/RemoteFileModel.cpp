// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "RemoteFileModel.h"

#include "gui/session/TerminalSessionWidget.h"

#include <QDateTime>
#include <QFileIconProvider>
#include <QLocale>

namespace
{
QFileIconProvider &iconProvider()
{
    static QFileIconProvider provider;
    return provider;
}
} // namespace

RemoteFileModel::RemoteFileModel(QObject *parent) : QAbstractItemModel(parent) {}

RemoteFileModel::~RemoteFileModel() = default;

void RemoteFileModel::bindSession(TerminalSessionWidget *session)
{
    if (m_session == session) {
        return;
    }

    if (m_session) {
        disconnect(m_session, nullptr, this, nullptr);
    }

    beginResetModel();
    m_session = session;
    m_root.reset();
    m_rootPath.clear();
    endResetModel();

    if (!m_session) {
        return;
    }

    connect(m_session,
            &TerminalSessionWidget::directoryListed,
            this,
            [this](const QString &path, const QVector<RemoteEntry> &entries) {
                applyListing(path, entries);
            });
}

void RemoteFileModel::unbindSession()
{
    bindSession(nullptr);
}

TerminalSessionWidget *RemoteFileModel::session() const
{
    return m_session;
}

void RemoteFileModel::setRootPath(const QString &path)
{
    beginResetModel();
    m_root = std::make_unique<Node>();
    m_root->entry.name = path;
    m_root->entry.path = path;
    m_root->entry.isDir = true;
    m_rootPath = path;
    endResetModel();

    requestList(m_root.get());
}

QString RemoteFileModel::rootPath() const
{
    return m_rootPath;
}

void RemoteFileModel::refresh(const QModelIndex &index)
{
    Node *node = index.isValid() ? nodeFromIndex(index) : m_root.get();
    if (!node || !node->entry.isDir) {
        if (m_root) {
            node = m_root.get();
        } else {
            return;
        }
    }

    const QModelIndex parentIndex = (node == m_root.get()) ? QModelIndex() : indexForNode(node);
    if (!node->children.empty()) {
        beginRemoveRows(parentIndex, 0, static_cast<int>(node->children.size()) - 1);
        node->children.clear();
        endRemoveRows();
    }

    node->fetched = false;
    node->fetching = false;
    node->populated = false;
    requestList(node);
}

void RemoteFileModel::refreshPath(const QString &path)
{
    if (!m_root) {
        return;
    }

    if (path == m_rootPath) {
        refresh({});
        return;
    }

    if (Node *node = findNodeByPath(m_root.get(), path)) {
        const QModelIndex idx = indexForNode(node);
        refresh(idx);
    }
}

QModelIndex RemoteFileModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_root || row < 0 || column < 0 || column >= columnCount()) {
        return {};
    }

    Node *parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!parentNode || row >= static_cast<int>(parentNode->children.size())) {
        return {};
    }

    return createIndex(row, column, parentNode->children.at(static_cast<size_t>(row)).get());
}

QModelIndex RemoteFileModel::parent(const QModelIndex &index) const
{
    if (!index.isValid() || !m_root) {
        return {};
    }

    Node *node = nodeFromIndex(index);
    if (!node || !node->parent || node->parent == m_root.get()) {
        return {};
    }

    Node *parentNode = node->parent;
    Node *grandParent = parentNode->parent;
    if (!grandParent) {
        return {};
    }

    for (int i = 0; i < static_cast<int>(grandParent->children.size()); ++i) {
        if (grandParent->children.at(static_cast<size_t>(i)).get() == parentNode) {
            return createIndex(i, 0, parentNode);
        }
    }
    return {};
}

int RemoteFileModel::rowCount(const QModelIndex &parent) const
{
    if (!m_root || parent.column() > 0) {
        return 0;
    }

    Node *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    return node ? static_cast<int>(node->children.size()) : 0;
}

int RemoteFileModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 4;
}

QVariant RemoteFileModel::data(const QModelIndex &index, int role) const
{
    Node *node = nodeFromIndex(index);
    if (!node) {
        return {};
    }

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0:
            return node->entry.name;
        case 1:
            return node->entry.isDir ? QVariant() : formatSize(node->entry.size);
        case 2:
            return node->entry.permissions;
        case 3:
            return node->entry.mtime > 0 ? formatMtime(node->entry.mtime) : QVariant();
        default:
            return {};
        }
    case Qt::DecorationRole:
        if (index.column() == 0) {
            return node->entry.isDir ? iconProvider().icon(QFileIconProvider::Folder)
                                     : iconProvider().icon(QFileIconProvider::File);
        }
        return {};
    case Qt::ToolTipRole:
        return node->entry.name;
    case PathRole:
        return node->entry.path;
    case IsDirRole:
        return node->entry.isDir;
    default:
        return {};
    }
}

QVariant RemoteFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case 0:
        return tr("Name");
    case 1:
        return tr("Size");
    case 2:
        return tr("Permissions");
    case 3:
        return tr("Modified");
    default:
        return {};
    }
}

Qt::ItemFlags RemoteFileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool RemoteFileModel::hasChildren(const QModelIndex &parent) const
{
    if (!m_root) {
        return false;
    }
    // Flat cwd listing: only the working directory has visible children.
    if (!parent.isValid()) {
        return !m_root->children.empty() || !m_root->populated;
    }
    return false;
}

bool RemoteFileModel::canFetchMore(const QModelIndex &parent) const
{
    if (!m_root || parent.isValid()) {
        return false;
    }

    Node *node = m_root.get();
    return node && node->entry.isDir && !node->fetched && !node->fetching;
}

void RemoteFileModel::fetchMore(const QModelIndex &parent)
{
    if (!m_root || parent.isValid()) {
        return;
    }

    Node *node = m_root.get();
    if (!node || !node->entry.isDir || node->fetched || node->fetching) {
        return;
    }

    requestList(node);
}

QString RemoteFileModel::pathForIndex(const QModelIndex &index) const
{
    Node *node = nodeFromIndex(index);
    return node ? node->entry.path : QString();
}

bool RemoteFileModel::isDirectory(const QModelIndex &index) const
{
    Node *node = nodeFromIndex(index);
    return node && node->entry.isDir;
}

bool RemoteFileModel::isParentNavEntry(const QModelIndex &index) const
{
    Node *node = nodeFromIndex(index);
    return node && node->entry.name == QLatin1String("..");
}

QModelIndex RemoteFileModel::indexForPath(const QString &path) const
{
    if (!m_root) {
        return {};
    }
    if (path == m_rootPath) {
        return {};
    }

    Node *node = findNodeByPath(m_root.get(), path);
    return node ? indexForNode(node) : QModelIndex();
}

QString RemoteFileModel::parentDirectory(const QModelIndex &index) const
{
    if (!index.isValid() || isParentNavEntry(index)) {
        return m_rootPath;
    }

    Node *node = nodeFromIndex(index);
    if (!node || !node->parent) {
        return m_rootPath;
    }
    return node->parent->entry.path;
}

QString RemoteFileModel::currentDirectory(const QModelIndex &index) const
{
    if (!index.isValid() || isParentNavEntry(index)) {
        return m_rootPath;
    }

    Node *node = nodeFromIndex(index);
    if (!node) {
        return m_rootPath;
    }
    return node->entry.isDir ? node->entry.path : node->parent->entry.path;
}

bool RemoteFileModel::hasChildNamed(const QString &name) const
{
    if (!m_root || name.isEmpty() || name == QLatin1String("..")) {
        return false;
    }

    for (const auto &child : m_root->children) {
        if (child && child->entry.name == name) {
            return true;
        }
    }
    return false;
}

QString RemoteFileModel::parentPathOf(const QString &path)
{
    if (path.isEmpty() || path == QLatin1String("/") || path == QLatin1String(".")) {
        return {};
    }

    QString cleaned = path;
    while (cleaned.size() > 1 && cleaned.endsWith(QLatin1Char('/'))) {
        cleaned.chop(1);
    }

    const int slash = cleaned.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return {};
    }
    if (slash == 0) {
        return QStringLiteral("/");
    }
    return cleaned.left(slash);
}

RemoteFileModel::Node *RemoteFileModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    return static_cast<Node *>(index.internalPointer());
}

QModelIndex RemoteFileModel::indexForNode(Node *node, int column) const
{
    if (!node || !node->parent) {
        return {};
    }

    for (int i = 0; i < static_cast<int>(node->parent->children.size()); ++i) {
        if (node->parent->children.at(static_cast<size_t>(i)).get() == node) {
            return createIndex(i, column, node);
        }
    }
    return {};
}

void RemoteFileModel::requestList(Node *node)
{
    if (!m_session || !node || node->fetching) {
        return;
    }

    node->fetching = true;
    m_session->listDirectory(node->entry.path);
}

void RemoteFileModel::applyListing(const QString &path, const QVector<RemoteEntry> &entries)
{
    if (!m_root) {
        return;
    }

    Node *node = (path == m_rootPath) ? m_root.get() : findNodeByPath(m_root.get(), path);
    if (!node) {
        return;
    }

    // Flat cwd browser: ignore stale listings for nested paths after navigation.
    if (node != m_root.get()) {
        return;
    }

    const QModelIndex parentIndex;

    if (!node->children.empty()) {
        beginRemoveRows(parentIndex, 0, static_cast<int>(node->children.size()) - 1);
        node->children.clear();
        endRemoveRows();
    }

    QVector<RemoteEntry> display;
    display.reserve(entries.size() + 1);
    for (const RemoteEntry &entry : entries) {
        if (!m_showHiddenFiles && entry.name.startsWith(QLatin1Char('.'))) {
            continue;
        }
        display.append(entry);
    }
    if (m_rootPath != QLatin1String("/")) {
        RemoteEntry up;
        up.name = QStringLiteral("..");
        up.path = parentPathOf(m_rootPath);
        if (up.path.isEmpty()) {
            up.path = QStringLiteral("/");
        }
        up.isDir = true;
        up.size = 0;
        display.prepend(up);
    }

    if (!display.isEmpty()) {
        beginInsertRows(parentIndex, 0, display.size() - 1);
        node->children.reserve(static_cast<size_t>(display.size()));
        for (const RemoteEntry &entry : display) {
            auto child = std::make_unique<Node>();
            child->entry = entry;
            child->parent = node;
            node->children.push_back(std::move(child));
        }
        endInsertRows();
    }

    node->fetched = true;
    node->fetching = false;
    node->populated = true;
}

RemoteFileModel::Node *RemoteFileModel::findNodeByPath(Node *node, const QString &path) const
{
    if (!node) {
        return nullptr;
    }
    if (node->entry.path == path) {
        return node;
    }

    for (const auto &child : node->children) {
        if (Node *found = findNodeByPath(child.get(), path)) {
            return found;
        }
    }
    return nullptr;
}

QString RemoteFileModel::formatSize(qint64 size)
{
    return QLocale().formattedDataSize(size);
}

QString RemoteFileModel::formatMtime(qint64 mtime)
{
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(mtime), QLocale::ShortFormat);
}

void RemoteFileModel::setShowHiddenFiles(bool show)
{
    if (m_showHiddenFiles == show) {
        return;
    }
    m_showHiddenFiles = show;
    if (!m_rootPath.isEmpty()) {
        refresh();
    }
}

bool RemoteFileModel::showHiddenFiles() const
{
    return m_showHiddenFiles;
}
