// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "OpenFileTracker.h"

#include "core/session/Session.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

OpenFileTracker::OpenFileTracker(QObject *parent)
    : QObject(parent), m_watcher(new QFileSystemWatcher(this)), m_debounce(new QTimer(this))
{
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(400);

    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &OpenFileTracker::onFileChanged);
    connect(m_watcher,
            &QFileSystemWatcher::directoryChanged,
            this,
            &OpenFileTracker::onDirectoryChanged);
    connect(m_debounce, &QTimer::timeout, this, &OpenFileTracker::flushPendingUploads);
}

void OpenFileTracker::track(const QString &localPath, const QString &remotePath, Session *session)
{
    if (localPath.isEmpty() || remotePath.isEmpty() || session == nullptr) {
        return;
    }

    const QFileInfo info(localPath);
    Entry entry;
    entry.remotePath = remotePath;
    entry.session = session;
    entry.parentDir = info.absolutePath();
    m_entries.insert(info.absoluteFilePath(), entry);

    ensureWatching(info.absoluteFilePath());
    if (!entry.parentDir.isEmpty() && !m_watcher->directories().contains(entry.parentDir)) {
        m_watcher->addPath(entry.parentDir);
    }
}

void OpenFileTracker::untrackSession(Session *session)
{
    if (session == nullptr) {
        return;
    }

    QStringList removeKeys;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (it.value().session == session) {
            removeKeys.append(it.key());
        }
    }

    for (const QString &key : removeKeys) {
        m_watcher->removePath(key);
        m_pendingUploads.remove(key);
        const QString parentDir = m_entries.value(key).parentDir;
        m_entries.remove(key);

        bool parentStillUsed = false;
        for (const Entry &entry : m_entries) {
            if (entry.parentDir == parentDir) {
                parentStillUsed = true;
                break;
            }
        }
        if (!parentStillUsed && !parentDir.isEmpty()) {
            m_watcher->removePath(parentDir);
        }
    }
}

void OpenFileTracker::clear()
{
    const QStringList files = m_watcher->files();
    if (!files.isEmpty()) {
        m_watcher->removePaths(files);
    }
    const QStringList dirs = m_watcher->directories();
    if (!dirs.isEmpty()) {
        m_watcher->removePaths(dirs);
    }
    m_entries.clear();
    m_pendingUploads.clear();
}

void OpenFileTracker::onFileChanged(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (!m_entries.contains(absolute)) {
        return;
    }

    // Editors often replace the inode on save; re-add if the watcher dropped it.
    ensureWatching(absolute);
    scheduleUpload(absolute);
}

void OpenFileTracker::onDirectoryChanged(const QString &path)
{
    // Catch atomic save (delete + recreate) via parent directory events.
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (it.value().parentDir == path) {
            ensureWatching(it.key());
            if (QFile::exists(it.key())) {
                scheduleUpload(it.key());
            }
        }
    }
}

void OpenFileTracker::flushPendingUploads()
{
    const QStringList paths = m_pendingUploads.keys();
    m_pendingUploads.clear();

    for (const QString &localPath : paths) {
        const Entry entry = m_entries.value(localPath);
        if (entry.session.isNull()) {
            m_entries.remove(localPath);
            continue;
        }
        if (!QFile::exists(localPath)) {
            ensureWatching(localPath);
            continue;
        }

        ensureWatching(localPath);
        emit statusMessage(tr("Uploading saved file: %1").arg(QFileInfo(localPath).fileName()),
                           ErrorNotifier::Level::Status);
        entry.session->uploadFileTo(localPath, entry.remotePath);
    }
}

void OpenFileTracker::ensureWatching(const QString &localPath)
{
    if (!QFile::exists(localPath)) {
        return;
    }
    if (!m_watcher->files().contains(localPath)) {
        m_watcher->addPath(localPath);
    }
}

void OpenFileTracker::scheduleUpload(const QString &localPath)
{
    m_pendingUploads.insert(localPath, true);
    m_debounce->start();
}
