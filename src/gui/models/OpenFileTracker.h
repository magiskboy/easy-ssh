/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "gui/ErrorNotifier.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QFileSystemWatcher;
class QTimer;
class Session;

class OpenFileTracker final : public QObject
{
    Q_OBJECT

public:
    explicit OpenFileTracker(QObject *parent = nullptr);

    void track(const QString &localPath, const QString &remotePath, Session *session);
    void untrackSession(Session *session);
    void clear();

signals:
    void statusMessage(const QString &message, ErrorNotifier::Level level);
    void syncFailed(const QString &message);

private slots:
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
    void flushPendingUploads();

private:
    struct Entry
    {
        QString remotePath;
        QPointer<Session> session;
        QString parentDir;
    };

    void ensureWatching(const QString &localPath);
    void scheduleUpload(const QString &localPath);

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
    QHash<QString, Entry> m_entries;
    QHash<QString, bool> m_pendingUploads;
};
