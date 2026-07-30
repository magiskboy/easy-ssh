// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "FileExplorerWidget.h"

#include "core/fs/SftpTypes.h"
#include "core/settings/AppSettings.h"
#include "gui/ErrorNotifier.h"
#include "gui/models/OpenFileTracker.h"
#include "gui/models/RemoteFileModel.h"
#include "gui/session/TerminalSessionWidget.h"

#include <QAbstractItemView>
#include <QAction>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSizePolicy>
#include <QStackedLayout>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

FileExplorerWidget::FileExplorerWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_openTracker = new OpenFileTracker(this);
    connect(
        m_openTracker, &OpenFileTracker::statusMessage, this, &FileExplorerWidget::statusMessage);
    connect(m_openTracker, &OpenFileTracker::syncFailed, this, [this](const QString &message) {
        ErrorNotifier::status(tr("Auto Sync: %1").arg(message), ErrorNotifier::Level::Warning);
    });

    m_refreshAction = new QAction(tr("Refresh"), this);
    m_refreshAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_refreshAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    m_uploadFilesAction = new QAction(tr("Upload Files…"), this);
    m_uploadFilesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+U")));
    m_uploadFilesAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    m_uploadFolderAction = new QAction(tr("Upload Folder…"), this);

    m_downloadAction = new QAction(tr("Download"), this);
    m_downloadAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    m_downloadAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    m_openWithAction = new QAction(tr("Open With…"), this);
    m_openWithAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    m_openWithAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    m_copyPathAction = new QAction(tr("Copy Full Path"), this);

    m_mkdirAction = new QAction(tr("New Folder"), this);

    m_renameAction = new QAction(tr("Rename"), this);
    m_renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    m_renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    m_deleteAction = new QAction(tr("Delete"), this);
    m_deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    m_deleteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    addAction(m_refreshAction);
    addAction(m_uploadFilesAction);
    addAction(m_uploadFolderAction);
    addAction(m_downloadAction);
    addAction(m_openWithAction);
    addAction(m_copyPathAction);
    addAction(m_mkdirAction);
    addAction(m_renameAction);
    addAction(m_deleteAction);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setContentsMargins(8, 4, 8, 4);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_pathLabel);

    m_stackHost = new QWidget(this);
    m_stackHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_stack = new QStackedLayout(m_stackHost);
    m_stack->setContentsMargins(0, 0, 0, 0);

    m_emptyLabel = new QLabel(tr("Connect to a session to browse remote files."), m_stackHost);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setEnabled(false);
    m_emptyLabel->setMargin(12);
    m_emptyLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_tree = new QTreeView(m_stackHost);
    m_model = new RemoteFileModel(this);
    m_tree->setModel(m_model);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(false);
    m_tree->setRootIsDecorated(false);
    m_tree->setItemsExpandable(false);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tree->setColumnWidth(1, 72);
    m_tree->setColumnWidth(2, 88);
    m_tree->setColumnWidth(3, 130);
    m_tree->viewport()->installEventFilter(this);

    m_stack->addWidget(m_emptyLabel);
    m_stack->addWidget(m_tree);
    layout->addWidget(m_stackHost, 1);

    // MobaXterm-style transfer strip at the bottom — non-modal, tree stays usable.
    m_transferBar = new QWidget(this);
    m_transferBar->setObjectName(QStringLiteral("transferBar"));
    auto *transferLayout = new QHBoxLayout(m_transferBar);
    transferLayout->setContentsMargins(8, 4, 8, 4);
    transferLayout->setSpacing(8);

    m_transferProgress = new QProgressBar(m_transferBar);
    m_transferProgress->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_transferProgress->setTextVisible(true);
    m_transferProgress->setAlignment(Qt::AlignCenter);
    m_transferProgress->setRange(0, 100);
    m_transferProgress->setValue(0);
    m_transferProgress->setFormat(tr("Transferring…"));
    m_transferProgress->installEventFilter(this);

    m_transferCancelButton = new QPushButton(tr("Cancel"), m_transferBar);
    m_transferCancelButton->setFlat(true);
    connect(m_transferCancelButton,
            &QPushButton::clicked,
            this,
            &FileExplorerWidget::cancelActiveTransfer);

    transferLayout->addWidget(m_transferProgress, 1);
    transferLayout->addWidget(m_transferCancelButton);
    m_transferBar->hide();
    layout->addWidget(m_transferBar);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    connect(m_refreshAction, &QAction::triggered, this, &FileExplorerWidget::refreshCurrent);
    connect(m_uploadFilesAction, &QAction::triggered, this, &FileExplorerWidget::uploadFiles);
    connect(m_uploadFolderAction, &QAction::triggered, this, &FileExplorerWidget::uploadFolder);
    connect(m_downloadAction, &QAction::triggered, this, &FileExplorerWidget::download);
    connect(m_openWithAction, &QAction::triggered, this, &FileExplorerWidget::openWith);
    connect(m_copyPathAction, &QAction::triggered, this, &FileExplorerWidget::copySelectedPath);
    connect(m_mkdirAction, &QAction::triggered, this, &FileExplorerWidget::createFolder);
    connect(m_renameAction, &QAction::triggered, this, &FileExplorerWidget::renameSelected);
    connect(m_deleteAction, &QAction::triggered, this, &FileExplorerWidget::deleteSelected);
    connect(m_tree,
            &QTreeView::customContextMenuRequested,
            this,
            &FileExplorerWidget::onCustomContextMenu);
    connect(m_tree, &QTreeView::activated, this, &FileExplorerWidget::onItemActivated);
    connect(m_tree->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [this](const QItemSelection &, const QItemSelection &) { updateActionsEnabled(); });

    auto *clearShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    clearShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(clearShortcut, &QShortcut::activated, this, &FileExplorerWidget::clearSelection);

    setIdleState(tr("Connect to a session to browse remote files."));
    updateActionsEnabled();
    applySettings();
    rebindShortcuts();
}

bool FileExplorerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tree->viewport() && event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton &&
            !m_tree->indexAt(mouse->position().toPoint()).isValid()) {
            clearSelection();
        }
    } else if (watched == m_transferProgress && event->type() == QEvent::Resize &&
               m_transferActive) {
        updateTransferProgressText();
    }
    return QWidget::eventFilter(watched, event);
}

void FileExplorerWidget::bindSession(TerminalSessionWidget *session)
{
    if (m_session == session && session != nullptr) {
        return;
    }

    if (m_session) {
        m_openTracker->untrackSession(m_session);
        disconnect(m_session, nullptr, this, nullptr);
    }

    m_session = session;
    m_pendingRootRequest.clear();
    m_pendingDeletes.clear();
    m_refreshAfterOp.clear();
    m_openWithQueue.clear();
    m_openWithActive = false;
    m_awaitingSftpResult = false;
    m_awaitingUploadListing = false;
    m_pendingUploadLocal.clear();
    m_pendingUploadRemoteDir.clear();
    finishTransferProgress();
    m_model->bindSession(session);

    if (!m_session) {
        updateSessionBadge();
        setIdleState(tr("Connect to a session to browse remote files."));
        updateActionsEnabled();
        return;
    }

    updateSessionBadge();

    connect(m_session,
            &TerminalSessionWidget::pathCanonicalized,
            this,
            &FileExplorerWidget::onPathCanonicalized);
    connect(
        m_session, &TerminalSessionWidget::sftpFinished, this, &FileExplorerWidget::onSftpFinished);
    connect(m_session, &TerminalSessionWidget::sftpError, this, &FileExplorerWidget::onSftpError);
    connect(
        m_session, &TerminalSessionWidget::sftpCanceled, this, &FileExplorerWidget::onSftpCanceled);
    connect(
        m_session, &TerminalSessionWidget::sftpProgress, this, &FileExplorerWidget::onSftpProgress);
    connect(m_session,
            &TerminalSessionWidget::directoryListed,
            this,
            &FileExplorerWidget::onDirectoryListed);
    connect(m_session,
            &TerminalSessionWidget::sftpUnavailable,
            this,
            [this](const QString &message) { showSftpUnavailable(message); });
    connect(m_session,
            &TerminalSessionWidget::sessionStateChanged,
            this,
            [this](TerminalSessionWidget::State state) {
                if (state != TerminalSessionWidget::State::Connected) {
                    unbindSession();
                }
            });

    if (!m_session->isSftpAvailable()) {
        showSftpUnavailable(m_session->sftpUnavailableReason().isEmpty()
                                ? tr("SFTP is unavailable on this session.")
                                : m_session->sftpUnavailableReason());
        return;
    }

    // Defer browse until after any queued sftpUnavailable from connect has been delivered.
    QTimer::singleShot(0, this, [this, session]() {
        if (m_session != session) {
            return;
        }
        if (!m_session->isSftpAvailable()) {
            showSftpUnavailable(m_session->sftpUnavailableReason().isEmpty()
                                    ? tr("SFTP is unavailable on this session.")
                                    : m_session->sftpUnavailableReason());
            return;
        }

        showTree(true);

        const QString startup = m_session->connection().startupDirectory.trimmed();
        m_pendingRootRequest = startup.isEmpty() ? QStringLiteral(".") : startup;
        m_pathLabel->setText(tr("Resolving %1…").arg(m_pendingRootRequest));
        m_awaitingSftpResult = true;
        setOpInFlight(true);
        m_session->canonicalizePath(m_pendingRootRequest);
        updateActionsEnabled();
    });
}

void FileExplorerWidget::unbindSession()
{
    if (m_session) {
        m_openTracker->untrackSession(m_session);
        disconnect(m_session, nullptr, this, nullptr);
    }
    m_session = nullptr;
    m_pendingRootRequest.clear();
    m_pendingDeletes.clear();
    m_refreshAfterOp.clear();
    m_openWithQueue.clear();
    m_openWithActive = false;
    m_awaitingSftpResult = false;
    m_awaitingUploadListing = false;
    m_pendingUploadLocal.clear();
    m_pendingUploadRemoteDir.clear();
    finishTransferProgress();
    m_model->unbindSession();
    setOpInFlight(false);
    updateSessionBadge();
    setIdleState(tr("Connect to a session to browse remote files."));
    updateActionsEnabled();
}

void FileExplorerWidget::setSessionBadge(const QString &name, const QString &detail)
{
    Q_UNUSED(name);
    Q_UNUSED(detail);
}

void FileExplorerWidget::updateSessionBadge()
{
    if (!m_session) {
        setSessionBadge(QString());
        return;
    }
    const Connection connection = m_session->connection();
    setSessionBadge(connection.name, connection.displayText());
}

void FileExplorerWidget::applySettings()
{
    const auto &settings = AppSettings::instance();
    if (m_tree) {
        m_tree->setColumnHidden(1, !settings.showSizeColumn());
        m_tree->setColumnHidden(2, !settings.showPermissionsColumn());
        m_tree->setColumnHidden(3, !settings.showModifiedColumn());
    }
    if (m_model) {
        m_model->setShowHiddenFiles(settings.showHiddenFiles());
    }
}

void FileExplorerWidget::rebindShortcuts()
{
    const auto &settings = AppSettings::instance();
    m_refreshAction->setShortcut(settings.shortcut(QStringLiteral("fileExplorer.refresh")));
    m_uploadFilesAction->setShortcut(settings.shortcut(QStringLiteral("fileExplorer.upload")));
    m_downloadAction->setShortcut(settings.shortcut(QStringLiteral("fileExplorer.download")));
    m_openWithAction->setShortcut(settings.shortcut(QStringLiteral("fileExplorer.openWith")));
    m_renameAction->setShortcut(settings.shortcut(QStringLiteral("fileExplorer.rename")));
    m_deleteAction->setShortcut(settings.shortcut(QStringLiteral("fileExplorer.delete")));
}

void FileExplorerWidget::refreshCurrent()
{
    if (!m_session || m_model->rootPath().isEmpty()) {
        return;
    }

    m_model->refresh({});
    emit statusMessage(tr("Refreshing…"), ErrorNotifier::Level::Status);
}

void FileExplorerWidget::uploadFiles()
{
    if (!m_session || m_opInFlight) {
        return;
    }
    if (targetDirectory().isEmpty()) {
        return;
    }

    const QStringList localPaths = QFileDialog::getOpenFileNames(this, tr("Upload Files"));
    startUpload(localPaths);
}

void FileExplorerWidget::uploadFolder()
{
    if (!m_session || m_opInFlight) {
        return;
    }
    if (targetDirectory().isEmpty()) {
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(this, tr("Upload Folder"));
    if (dir.isEmpty()) {
        return;
    }
    startUpload({dir});
}

void FileExplorerWidget::startUpload(const QStringList &localPaths)
{
    if (!m_session || m_opInFlight || localPaths.isEmpty()) {
        return;
    }

    const QString remoteDir = targetDirectory();
    if (remoteDir.isEmpty()) {
        return;
    }

    if (remoteDir == m_model->rootPath()) {
        QStringList conflicts;
        for (const QString &localPath : localPaths) {
            const QString name = QFileInfo(localPath).fileName();
            if (m_model->hasChildNamed(name)) {
                conflicts.append(name);
            }
        }
        confirmConflictsAndUpload(localPaths, remoteDir, conflicts);
        return;
    }

    // Target is a selected subdirectory — list it first to detect name clashes.
    m_pendingUploadLocal = localPaths;
    m_pendingUploadRemoteDir = remoteDir;
    m_awaitingUploadListing = true;
    setOpInFlight(true);
    emit statusMessage(tr("Checking for existing files in %1…").arg(remoteDir),
                       ErrorNotifier::Level::Status);
    m_session->listDirectory(remoteDir);
}

void FileExplorerWidget::confirmConflictsAndUpload(const QStringList &localPaths,
                                                   const QString &remoteDir,
                                                   const QStringList &conflicts)
{
    if (!conflicts.isEmpty()) {
        const QString detail = conflicts.size() <= 8
                                   ? conflicts.join(QLatin1Char('\n'))
                                   : conflicts.mid(0, 8).join(QLatin1Char('\n')) +
                                         tr("\n…and %1 more").arg(conflicts.size() - 8);

        const auto answer = QMessageBox::warning(
            this,
            tr("Overwrite Existing Files"),
            tr("The following item(s) already exist on the server and will be overwritten:\n\n"
               "%1\n\nDo you want to continue?")
                .arg(detail),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            m_pendingUploadLocal.clear();
            m_pendingUploadRemoteDir.clear();
            m_awaitingUploadListing = false;
            setOpInFlight(false);
            emit statusMessage(tr("Upload canceled"), ErrorNotifier::Level::Warning);
            return;
        }
    }

    beginUpload(localPaths, remoteDir);
}

void FileExplorerWidget::beginUpload(const QStringList &localPaths, const QString &remoteDir)
{
    m_pendingUploadLocal.clear();
    m_pendingUploadRemoteDir.clear();
    m_awaitingUploadListing = false;

    setOpInFlight(true);
    m_awaitingSftpResult = true;
    startTransferProgress(tr("Uploading…"));
    emit statusMessage(tr("Uploading to %1…").arg(remoteDir), ErrorNotifier::Level::Status);
    m_refreshAfterOp = remoteDir;
    m_session->uploadFiles(localPaths, remoteDir);
}

QStringList FileExplorerWidget::conflictNamesInEntries(const QStringList &localPaths,
                                                       const QVector<RemoteEntry> &entries) const
{
    QSet<QString> existing;
    existing.reserve(entries.size());
    for (const RemoteEntry &entry : entries) {
        if (entry.name != QLatin1String(".") && entry.name != QLatin1String("..")) {
            existing.insert(entry.name);
        }
    }

    QStringList conflicts;
    for (const QString &localPath : localPaths) {
        const QString name = QFileInfo(localPath).fileName();
        if (existing.contains(name)) {
            conflicts.append(name);
        }
    }
    return conflicts;
}

void FileExplorerWidget::onDirectoryListed(const QString &path, const QVector<RemoteEntry> &entries)
{
    if (!m_awaitingUploadListing || path != m_pendingUploadRemoteDir) {
        return;
    }

    m_awaitingUploadListing = false;
    const QStringList localPaths = m_pendingUploadLocal;
    const QString remoteDir = m_pendingUploadRemoteDir;
    // Keep opInFlight true through the confirmation dialog.
    confirmConflictsAndUpload(localPaths, remoteDir, conflictNamesInEntries(localPaths, entries));
}

void FileExplorerWidget::download()
{
    if (!m_session || m_opInFlight) {
        return;
    }

    const QStringList remotePaths = selectedRemotePaths();
    if (remotePaths.isEmpty()) {
        ErrorNotifier::status(tr("Select one or more items to download."),
                              ErrorNotifier::Level::Warning);
        return;
    }

    QString localDir = AppSettings::instance().defaultDownloadDir().trimmed();
    if (localDir.isEmpty() || !QDir(localDir).exists()) {
        localDir = QFileDialog::getExistingDirectory(this, tr("Download To"));
        if (localDir.isEmpty()) {
            return;
        }
    }

    setOpInFlight(true);
    m_awaitingSftpResult = true;
    startTransferProgress(tr("Downloading…"));
    emit statusMessage(tr("Downloading to %1…").arg(localDir), ErrorNotifier::Level::Status);
    m_refreshAfterOp.clear();
    m_session->downloadPaths(remotePaths, localDir);
}

void FileExplorerWidget::openWith()
{
    if (!m_session || m_opInFlight) {
        return;
    }

    const QStringList remoteFiles = selectedRemoteFiles();
    if (remoteFiles.isEmpty()) {
        ErrorNotifier::status(tr("Select one or more files to open locally."),
                              ErrorNotifier::Level::Warning);
        return;
    }

    m_openWithQueue.clear();
    for (const QString &remotePath : remoteFiles) {
        const QByteArray hash =
            QCryptographicHash::hash(remotePath.toUtf8(), QCryptographicHash::Sha1)
                .toHex()
                .left(12);
        const QString localDir = QDir(openWithTempRoot()).filePath(QString::fromLatin1(hash));
        if (!QDir().mkpath(localDir)) {
            ErrorNotifier::notify(this,
                                  tr("Open With"),
                                  tr("Cannot create temporary directory."),
                                  ErrorNotifier::Level::Warning);
            m_openWithQueue.clear();
            return;
        }

        OpenWithItem item;
        item.remotePath = remotePath;
        item.localDir = localDir;
        item.localPath = QDir(localDir).filePath(QFileInfo(remotePath).fileName());
        m_openWithQueue.append(item);
    }

    m_openWithActive = true;
    m_awaitingSftpResult = true;
    setOpInFlight(true);
    startTransferProgress(tr("Opening…"));
    startNextOpenWithDownload();
}

void FileExplorerWidget::startNextOpenWithDownload()
{
    if (!m_session || m_openWithQueue.isEmpty()) {
        m_openWithActive = false;
        finishTransferProgress();
        setOpInFlight(false);
        updateActionsEnabled();
        return;
    }

    const OpenWithItem &item = m_openWithQueue.first();
    emit statusMessage(tr("Downloading %1 for editing…").arg(QFileInfo(item.remotePath).fileName()),
                       ErrorNotifier::Level::Status);
    m_refreshAfterOp.clear();
    m_session->downloadPaths({item.remotePath}, item.localDir);
}

void FileExplorerWidget::completeOpenWithItem()
{
    if (m_openWithQueue.isEmpty() || !m_session) {
        return;
    }

    const OpenWithItem item = m_openWithQueue.takeFirst();
    if (!QFileInfo::exists(item.localPath)) {
        ErrorNotifier::notify(this,
                              tr("Open With"),
                              tr("Downloaded file is missing: %1").arg(item.localPath),
                              ErrorNotifier::Level::Warning);
    } else {
        m_openTracker->track(item.localPath, item.remotePath, m_session);
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(item.localPath))) {
            ErrorNotifier::notify(
                this,
                tr("Open With"),
                tr("Could not open %1 with the default application.").arg(item.localPath),
                ErrorNotifier::Level::Warning);
        } else {
            emit statusMessage(tr("Opened %1 — saves will sync automatically.")
                                   .arg(QFileInfo(item.localPath).fileName()),
                               ErrorNotifier::Level::Success);
        }
    }

    if (!m_openWithQueue.isEmpty()) {
        startNextOpenWithDownload();
        return;
    }

    m_openWithActive = false;
    m_awaitingSftpResult = false;
    finishTransferProgress();
    setOpInFlight(false);
    updateActionsEnabled();
}

QString FileExplorerWidget::openWithTempRoot() const
{
    const QString sessionKey = m_session
                                   ? QString::number(reinterpret_cast<quintptr>(m_session), 16)
                                   : QStringLiteral("none");
    return QDir::temp().filePath(QStringLiteral("easy-ssh/%1").arg(sessionKey));
}

void FileExplorerWidget::createFolder()
{
    if (!m_session || m_opInFlight) {
        return;
    }

    const QString remoteDir = targetDirectory();
    if (remoteDir.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString name =
        QInputDialog::getText(
            this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal, QString(), &ok)
            .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) {
        QMessageBox::warning(
            this, tr("New Folder"), tr("Folder name cannot contain path separators."));
        return;
    }

    const QString path = remoteDir.endsWith(QLatin1Char('/')) ? remoteDir + name
                                                              : remoteDir + QLatin1Char('/') + name;

    setOpInFlight(true);
    m_awaitingSftpResult = true;
    m_refreshAfterOp = remoteDir;
    m_session->createDirectory(path);
}

void FileExplorerWidget::renameSelected()
{
    if (!m_session || m_opInFlight) {
        return;
    }

    const QModelIndex index = currentIndex();
    if (!index.isValid() || m_model->isParentNavEntry(index)) {
        return;
    }

    const QString from = m_model->pathForIndex(index);
    const QString oldName = index.siblingAtColumn(0).data().toString();
    bool ok = false;
    const QString newName =
        QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, oldName, &ok)
            .trimmed();
    if (!ok || newName.isEmpty() || newName == oldName) {
        return;
    }
    if (newName.contains(QLatin1Char('/')) || newName.contains(QLatin1Char('\\')) ||
        newName == QLatin1String("..") || newName == QLatin1String(".")) {
        QMessageBox::warning(this, tr("Rename"), tr("Name cannot contain path separators."));
        return;
    }

    const QString parentDir = m_model->parentDirectory(index);
    const QString to = parentDir.endsWith(QLatin1Char('/'))
                           ? parentDir + newName
                           : parentDir + QLatin1Char('/') + newName;

    setOpInFlight(true);
    m_awaitingSftpResult = true;
    m_refreshAfterOp = parentDir;
    m_session->renamePath(from, to);
}

void FileExplorerWidget::deleteSelected()
{
    if (!m_session || m_opInFlight) {
        return;
    }

    const QStringList paths = selectedRemotePaths();
    if (paths.isEmpty()) {
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        tr("Delete"),
        tr("Delete %1 selected item(s)?\nThis cannot be undone.").arg(paths.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    m_pendingDeletes = paths;
    m_refreshAfterOp = m_model->parentDirectory(currentIndex());
    m_awaitingSftpResult = true;
    setOpInFlight(true);
    deleteNextPending();
}

void FileExplorerWidget::deleteNextPending()
{
    if (!m_session || m_pendingDeletes.isEmpty()) {
        setOpInFlight(false);
        return;
    }

    const QString path = m_pendingDeletes.first();
    emit statusMessage(tr("Deleting %1…").arg(path), ErrorNotifier::Level::Status);
    m_session->removePath(path, true);
}

void FileExplorerWidget::onPathCanonicalized(const QString &requested, const QString &canonical)
{
    if (m_pendingRootRequest.isEmpty()) {
        return;
    }
    if (requested != m_pendingRootRequest && canonical != m_pendingRootRequest) {
        return;
    }

    m_pendingRootRequest.clear();
    m_awaitingSftpResult = false;
    navigateTo(canonical);
    emit statusMessage(tr("Browsing %1").arg(canonical), ErrorNotifier::Level::Status);
}

void FileExplorerWidget::navigateTo(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    m_model->setRootPath(path);
    m_pathLabel->setText(path);
    showTree(true);
    clearSelection();
    setOpInFlight(false);
    updateActionsEnabled();
}

void FileExplorerWidget::onItemActivated(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    if (m_model->isDirectory(index)) {
        const QString path = m_model->pathForIndex(index);
        if (path.isEmpty()) {
            return;
        }

        navigateTo(path);
        emit statusMessage(tr("Browsing %1").arg(path), ErrorNotifier::Level::Status);
        return;
    }

    if (m_model->isParentNavEntry(index)) {
        return;
    }

    // Ensure the activated file is selected so openWith() uses it.
    if (m_tree->selectionModel() && !m_tree->selectionModel()->isSelected(index)) {
        m_tree->selectionModel()->select(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_tree->setCurrentIndex(index);
    }

    openWith();
}

void FileExplorerWidget::clearSelection()
{
    if (!m_tree || !m_tree->selectionModel()) {
        return;
    }
    m_tree->selectionModel()->clearSelection();
    m_tree->setCurrentIndex({});
    updateActionsEnabled();
}

void FileExplorerWidget::onSftpFinished(const QString &message)
{
    emit statusMessage(message, ErrorNotifier::Level::Success);

    if (m_openWithActive) {
        completeOpenWithItem();
        return;
    }

    // Auto-sync from OpenFileTracker — ignore for explorer busy/progress state.
    if (!m_awaitingSftpResult) {
        return;
    }

    if (!m_pendingDeletes.isEmpty()) {
        m_pendingDeletes.removeFirst();
        if (!m_pendingDeletes.isEmpty()) {
            deleteNextPending();
            return;
        }
    }

    m_awaitingSftpResult = false;
    finishTransferProgress();
    setOpInFlight(false);

    if (!m_refreshAfterOp.isEmpty()) {
        m_model->refreshPath(m_refreshAfterOp);
        m_refreshAfterOp.clear();
    }
    updateActionsEnabled();
}

void FileExplorerWidget::onSftpError(const QString &message)
{
    // Prefer the Files idle warning over a modal when SFTP was never brought up.
    if (m_session && !m_session->isSftpAvailable()) {
        showSftpUnavailable(m_session->sftpUnavailableReason().isEmpty()
                                ? message
                                : m_session->sftpUnavailableReason());
        return;
    }

    if (m_awaitingUploadListing) {
        m_awaitingUploadListing = false;
        m_pendingUploadLocal.clear();
        m_pendingUploadRemoteDir.clear();
        setOpInFlight(false);
        ErrorNotifier::notify(this,
                              tr("Upload"),
                              tr("Cannot check destination folder:\n%1").arg(message),
                              ErrorNotifier::Level::Warning);
        updateActionsEnabled();
        return;
    }

    if (!m_awaitingSftpResult && !m_openWithActive) {
        ErrorNotifier::status(tr("Auto Sync: %1").arg(message), ErrorNotifier::Level::Warning);
        return;
    }

    m_pendingDeletes.clear();
    m_refreshAfterOp.clear();
    m_openWithQueue.clear();
    m_openWithActive = false;
    m_awaitingSftpResult = false;
    finishTransferProgress();
    setOpInFlight(false);
    ErrorNotifier::status(tr("SFTP: %1").arg(message), ErrorNotifier::Level::Error);
    updateActionsEnabled();
}

void FileExplorerWidget::onSftpCanceled(const QString &message)
{
    m_pendingDeletes.clear();
    m_refreshAfterOp.clear();
    m_openWithQueue.clear();
    m_openWithActive = false;
    m_awaitingSftpResult = false;
    finishTransferProgress();
    setOpInFlight(false);
    emit statusMessage(message, ErrorNotifier::Level::Warning);
    updateActionsEnabled();
}

void FileExplorerWidget::onSftpProgress(qint64 bytesDone,
                                        qint64 bytesTotal,
                                        const QString &currentName)
{
    if (!m_transferActive || !m_transferProgress) {
        return;
    }

    m_transferName = currentName.isEmpty() ? tr("Transferring") : currentName;
    m_transferBytesDone = bytesDone;
    m_transferBytesTotal = bytesTotal;

    if (bytesTotal > 0) {
        m_transferProgress->setRange(0, 100);
        const int percent = qBound(0, static_cast<int>((bytesDone * 100) / bytesTotal), 100);
        m_transferProgress->setValue(percent);
    } else {
        m_transferProgress->setRange(0, 0);
    }

    updateTransferProgressText();
}

void FileExplorerWidget::updateTransferProgressText()
{
    if (!m_transferProgress) {
        return;
    }

    const QFontMetrics metrics(m_transferProgress->font());
    const int available = qMax(40, m_transferProgress->width() - 24);

    if (m_transferBytesTotal > 0) {
        const int percent =
            qBound(0, static_cast<int>((m_transferBytesDone * 100) / m_transferBytesTotal), 100);
        const QString suffix =
            tr(" — %1% (%2 / %3)")
                .arg(percent)
                .arg(formatByteSize(m_transferBytesDone), formatByteSize(m_transferBytesTotal));
        const int nameWidth = qMax(24, available - metrics.horizontalAdvance(suffix));
        const QString name = elideMiddle(m_transferName, metrics, nameWidth);
        m_transferProgress->setFormat(name + suffix);
        return;
    }

    if (m_transferBytesDone > 0) {
        const QString suffix = tr(" — %1").arg(formatByteSize(m_transferBytesDone));
        const int nameWidth = qMax(24, available - metrics.horizontalAdvance(suffix));
        m_transferProgress->setFormat(elideMiddle(m_transferName, metrics, nameWidth) + suffix);
        return;
    }

    m_transferProgress->setFormat(elideMiddle(m_transferName, metrics, available));
}

void FileExplorerWidget::startTransferProgress(const QString &label)
{
    m_transferActive = true;
    m_transferName = label;
    m_transferBytesDone = 0;
    m_transferBytesTotal = -1;
    m_transferProgress->setRange(0, 100);
    m_transferProgress->setValue(0);
    updateTransferProgressText();
    m_transferCancelButton->setEnabled(true);
    m_transferBar->show();
}

void FileExplorerWidget::finishTransferProgress()
{
    m_transferActive = false;
    m_transferName.clear();
    m_transferBytesDone = 0;
    m_transferBytesTotal = -1;
    if (m_transferBar) {
        m_transferBar->hide();
        m_transferProgress->setRange(0, 100);
        m_transferProgress->setValue(0);
        m_transferProgress->setFormat(QString());
    }
}

void FileExplorerWidget::cancelActiveTransfer()
{
    if (!m_session || !m_transferActive) {
        return;
    }
    m_transferCancelButton->setEnabled(false);
    m_transferName = tr("Canceling…");
    m_transferBytesDone = 0;
    m_transferBytesTotal = -1;
    updateTransferProgressText();
    m_session->cancelTransfer();
}

void FileExplorerWidget::onCustomContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_tree->indexAt(pos);
    if (index.isValid() && !m_tree->selectionModel()->isSelected(index)) {
        m_tree->selectionModel()->select(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_tree->setCurrentIndex(index);
    }

    QMenu menu(this);
    menu.addAction(m_refreshAction);
    menu.addSeparator();
    menu.addAction(m_uploadFilesAction);
    menu.addAction(m_uploadFolderAction);
    menu.addAction(m_downloadAction);
    menu.addAction(m_openWithAction);
    menu.addAction(m_copyPathAction);
    menu.addSeparator();
    menu.addAction(m_mkdirAction);
    menu.addAction(m_renameAction);
    menu.addAction(m_deleteAction);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void FileExplorerWidget::copySelectedPath()
{
    const QModelIndex index = currentIndex();
    if (!index.isValid()) {
        return;
    }

    const QString path = m_model->pathForIndex(index);
    if (path.isEmpty()) {
        return;
    }

    QGuiApplication::clipboard()->setText(path);
    emit statusMessage(tr("Copied path: %1").arg(path), ErrorNotifier::Level::Success);
}

void FileExplorerWidget::updateActionsEnabled()
{
    const bool connected = m_session &&
                           m_session->sessionState() == TerminalSessionWidget::State::Connected &&
                           !m_model->rootPath().isEmpty();
    const QModelIndex index = currentIndex();
    const bool hasSelection = index.isValid() && !m_model->isParentNavEntry(index);
    const bool hasFileSelection = !selectedRemoteFiles().isEmpty();
    const bool canMutate = connected && !m_opInFlight;

    m_refreshAction->setEnabled(connected);
    m_uploadFilesAction->setEnabled(canMutate);
    m_uploadFolderAction->setEnabled(canMutate);
    m_downloadAction->setEnabled(canMutate && hasSelection);
    m_openWithAction->setEnabled(canMutate && hasFileSelection);
    m_copyPathAction->setEnabled(connected && index.isValid());
    m_mkdirAction->setEnabled(canMutate);
    m_renameAction->setEnabled(canMutate && hasSelection);
    m_deleteAction->setEnabled(canMutate && !selectedRemotePaths().isEmpty());
}

QModelIndex FileExplorerWidget::currentIndex() const
{
    if (!m_tree || !m_tree->selectionModel() || !m_tree->selectionModel()->hasSelection()) {
        return {};
    }

    const QModelIndex current = m_tree->selectionModel()->currentIndex();
    if (current.isValid() && m_tree->selectionModel()->isSelected(current)) {
        return current.siblingAtColumn(0);
    }

    const auto rows = m_tree->selectionModel()->selectedRows(0);
    return rows.isEmpty() ? QModelIndex() : rows.first();
}

QString FileExplorerWidget::targetDirectory() const
{
    // No selection (or "..") → current working directory.
    return m_model->currentDirectory(currentIndex());
}

QStringList FileExplorerWidget::selectedRemotePaths() const
{
    QStringList paths;
    if (!m_tree || !m_tree->selectionModel()) {
        return paths;
    }

    const auto indexes = m_tree->selectionModel()->selectedRows(0);
    for (const QModelIndex &index : indexes) {
        if (m_model->isParentNavEntry(index)) {
            continue;
        }
        const QString path = m_model->pathForIndex(index);
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

QStringList FileExplorerWidget::selectedRemoteFiles() const
{
    QStringList paths;
    if (!m_tree || !m_tree->selectionModel()) {
        return paths;
    }

    const auto indexes = m_tree->selectionModel()->selectedRows(0);
    for (const QModelIndex &index : indexes) {
        if (m_model->isDirectory(index) || m_model->isParentNavEntry(index)) {
            continue;
        }
        const QString path = m_model->pathForIndex(index);
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

void FileExplorerWidget::setIdleState(const QString &message)
{
    m_pathLabel->setText(QString());
    m_emptyLabel->setText(message);
    showTree(false);
}

void FileExplorerWidget::setOpInFlight(bool inFlight)
{
    m_opInFlight = inFlight;
    // Keep the tree interactive during background transfers (browse/select).
    const bool treeOk = m_session != nullptr && !m_model->rootPath().isEmpty();
    m_tree->setEnabled(treeOk);
    updateActionsEnabled();
}

void FileExplorerWidget::showTree(bool show)
{
    if (!m_stack) {
        return;
    }
    m_stack->setCurrentWidget(show ? static_cast<QWidget *>(m_tree)
                                   : static_cast<QWidget *>(m_emptyLabel));
}

void FileExplorerWidget::showSftpUnavailable(const QString &message)
{
    m_pendingRootRequest.clear();
    m_pendingDeletes.clear();
    m_refreshAfterOp.clear();
    m_openWithQueue.clear();
    m_openWithActive = false;
    m_awaitingSftpResult = false;
    finishTransferProgress();
    setOpInFlight(false);
    m_model->unbindSession();
    // Keep m_session so tab switches / disconnect still work via sessionStateChanged.
    const QString detail = message.isEmpty() ? tr("This server does not support SFTP.") : message;
    m_pathLabel->setText(tr("SFTP unavailable"));
    m_emptyLabel->setText(tr("%1\n\nTerminal session is still active.").arg(detail));
    showTree(false);
    emit statusMessage(detail, ErrorNotifier::Level::Warning);
    updateActionsEnabled();
}

QString FileExplorerWidget::formatByteSize(qint64 bytes)
{
    constexpr qint64 kKb = 1024;
    constexpr qint64 kMb = 1024 * kKb;
    constexpr qint64 kGb = 1024 * kMb;
    if (bytes >= kGb) {
        return QStringLiteral("%1 GB").arg(bytes / static_cast<double>(kGb), 0, 'f', 2);
    }
    if (bytes >= kMb) {
        return QStringLiteral("%1 MB").arg(bytes / static_cast<double>(kMb), 0, 'f', 1);
    }
    if (bytes >= kKb) {
        return QStringLiteral("%1 KB").arg(bytes / static_cast<double>(kKb), 0, 'f', 0);
    }
    return QStringLiteral("%1 B").arg(bytes);
}

QString
FileExplorerWidget::elideMiddle(const QString &text, const QFontMetrics &metrics, int maxWidth)
{
    if (maxWidth <= 0 || text.isEmpty()) {
        return text;
    }
    return metrics.elidedText(text, Qt::ElideMiddle, maxWidth);
}
