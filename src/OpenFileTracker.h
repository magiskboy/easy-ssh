#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QFileSystemWatcher;
class QTimer;
class TerminalSessionWidget;

class OpenFileTracker final : public QObject
{
    Q_OBJECT

public:
    explicit OpenFileTracker(QObject *parent = nullptr);

    void track(const QString &localPath, const QString &remotePath, TerminalSessionWidget *session);
    void untrackSession(TerminalSessionWidget *session);
    void clear();

signals:
    void statusMessage(const QString &message);
    void syncFailed(const QString &message);

private slots:
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
    void flushPendingUploads();

private:
    struct Entry
    {
        QString remotePath;
        QPointer<TerminalSessionWidget> session;
        QString parentDir;
    };

    void ensureWatching(const QString &localPath);
    void scheduleUpload(const QString &localPath);

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
    QHash<QString, Entry> m_entries;
    QHash<QString, bool> m_pendingUploads;
};
