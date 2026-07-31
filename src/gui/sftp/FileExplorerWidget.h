/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/fs/SftpTypes.h"
#include "gui/ErrorNotifier.h"

#include <QList>
#include <QModelIndex>
#include <QStringList>
#include <QWidget>

class QAction;
class QEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QTreeView;
class OpenFileTracker;
class RemoteFileModel;
class Session;

class FileExplorerWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit FileExplorerWidget(QWidget *parent = nullptr);

    void bindSession(Session *session);
    void unbindSession();
    void applySettings();
    void rebindShortcuts();

signals:
    void statusMessage(const QString &message, ErrorNotifier::Level level);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void refreshCurrent();
    void uploadFiles();
    void uploadFolder();
    void download();
    void openWith();
    void createFolder();
    void renameSelected();
    void deleteSelected();
    void onPathCanonicalized(const QString &requested, const QString &canonical);
    void onSftpFinished(const QString &message);
    void onSftpError(const QString &message);
    void onSftpCanceled(const QString &message);
    void onSftpProgress(qint64 bytesDone, qint64 bytesTotal, const QString &currentName);
    void onDirectoryListed(const QString &path, const QVector<RemoteEntry> &entries);
    void onCustomContextMenu(const QPoint &pos);
    void onItemActivated(const QModelIndex &index);
    void clearSelection();
    void copySelectedPath();
    void updateActionsEnabled();
    void cancelActiveTransfer();

private:
    struct OpenWithItem
    {
        QString remotePath;
        QString localDir;
        QString localPath;
    };

    QModelIndex currentIndex() const;
    QString targetDirectory() const;
    QStringList selectedRemotePaths() const;
    QStringList selectedRemoteFiles() const;
    void startUpload(const QStringList &localPaths);
    void confirmConflictsAndUpload(const QStringList &localPaths,
                                   const QString &remoteDir,
                                   const QStringList &conflicts);
    void beginUpload(const QStringList &localPaths, const QString &remoteDir);
    QStringList conflictNamesInEntries(const QStringList &localPaths,
                                       const QVector<RemoteEntry> &entries) const;
    void startTransferProgress(const QString &label);
    void finishTransferProgress();
    void updateTransferProgressText();
    void setIdleState(const QString &message);
    void setOpInFlight(bool inFlight);
    void showTree(bool show);
    void showSftpUnavailable(const QString &message);
    void deleteNextPending();
    void startNextOpenWithDownload();
    void completeOpenWithItem();
    void navigateTo(const QString &path);
    void setSessionBadge();
    void updateSessionBadge();
    QString openWithTempRoot() const;
    static QString formatByteSize(qint64 bytes);
    static QString elideMiddle(const QString &text, const QFontMetrics &metrics, int maxWidth);

    RemoteFileModel *m_model = nullptr;
    QTreeView *m_tree = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QWidget *m_stackHost = nullptr;
    class QStackedLayout *m_stack = nullptr;
    QWidget *m_transferBar = nullptr;
    QProgressBar *m_transferProgress = nullptr;
    QPushButton *m_transferCancelButton = nullptr;
    OpenFileTracker *m_openTracker = nullptr;
    QAction *m_refreshAction = nullptr;
    QAction *m_uploadFilesAction = nullptr;
    QAction *m_uploadFolderAction = nullptr;
    QAction *m_downloadAction = nullptr;
    QAction *m_openWithAction = nullptr;
    QAction *m_copyPathAction = nullptr;
    QAction *m_mkdirAction = nullptr;
    QAction *m_renameAction = nullptr;
    QAction *m_deleteAction = nullptr;
    Session *m_session = nullptr;
    QString m_pendingRootRequest;
    QString m_refreshAfterOp;
    QStringList m_pendingDeletes;
    QList<OpenWithItem> m_openWithQueue;
    QStringList m_pendingUploadLocal;
    QString m_pendingUploadRemoteDir;
    QString m_transferName;
    qint64 m_transferBytesDone = 0;
    qint64 m_transferBytesTotal = -1;
    bool m_opInFlight = false;
    bool m_transferActive = false;
    bool m_openWithActive = false;
    bool m_awaitingSftpResult = false;
    bool m_awaitingUploadListing = false;
};
