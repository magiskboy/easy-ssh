// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SshWorker.h"

#include "core/fs/ShellExecRunner.h"
#include "core/fs/TransferJobStore.h"
#include "core/settings/AppSettings.h"
#include "core/ssh/AgentForwardHost.h"
#include "core/ssh/SftpMetaIoHandler.h"
#include "core/ssh/SftpTransferIoHandler.h"
#include "core/tunnel/RemoteTunnelSession.h"
#include "core/util/Logging.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QMetaObject>
#include <QScopeGuard>
#include <QTimer>

#include <algorithm>
#include <libssh/libssh.h>
#include <memory>
#include <utility>

namespace
{
void filterTransferPartials(QVector<RemoteEntry> *entries)
{
    if (!entries) {
        return;
    }
    entries->erase(
        std::remove_if(entries->begin(),
                       entries->end(),
                       [](const RemoteEntry &entry) { return isTransferFilepartName(entry.name); }),
        entries->end());
}
} // namespace

SshWorker::SshWorker(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<TransferJob>("TransferJob");
    m_fs.setProgressCallback([this](qint64 done, qint64 total, const QString &name) {
        emit sftpProgress(done, total, name);
    });

    m_session.setHostKeyVerifier([this](ssh_session session, const QString &contextLabel) {
        return verifyKnownHostForSession(session, contextLabel);
    });
    m_session.setCancelChecker([this]() { return m_cancelRequested.load(); });

    connect(&m_ioLoop, &SshIoLoop::fault, this, &SshWorker::onIoLoopFault);
    connect(&m_ioLoop, &SshIoLoop::sessionEof, this, &SshWorker::onIoLoopSessionEof);
}

SshWorker::~SshWorker()
{
    cleanup();
}

void SshWorker::connectToHost(const Connection &connection,
                              const SessionCredentials &credentials,
                              const QUuid &initialShellId,
                              int cols,
                              int rows)
{
    if (m_running) {
        emit errorOccurred(tr("Session already connected"));
        return;
    }

    if (initialShellId.isNull()) {
        emit errorOccurred(tr("Initial shell id is required"));
        return;
    }

    m_cancelRequested.store(false);
    cleanup();

    m_agentForwarding = connection.agentForwarding;

    QString error;
    if (!m_session.establish(connection, credentials, &error)) {
        if (m_cancelRequested.load()) {
            return;
        }
        if (!error.isEmpty()) {
            emit errorOccurred(error);
        }
        return;
    }

    if (m_cancelRequested.load()) {
        cleanup();
        return;
    }

    QString attachError;
    if (!m_ioLoop.attachSession(m_session.handle(), &attachError)) {
        emit errorOccurred(attachError.isEmpty() ? tr("Failed to attach SSH I/O loop")
                                                 : attachError);
        m_session.cleanup();
        return;
    }

    if (!openShellLocked(initialShellId, cols, rows, &error)) {
        if (m_cancelRequested.load()) {
            cleanup();
            return;
        }
        if (!error.isEmpty()) {
            emit errorOccurred(error);
        }
        m_ioLoop.detachSession();
        m_session.cleanup();
        return;
    }

    if (m_cancelRequested.load()) {
        cleanup();
        return;
    }

    QString sftpFailure;
    m_fs.setConnectionId(connection.id);
    m_fs.setStallTimeoutSec(AppSettings::instance().transferStallTimeoutSec());
    m_fs.setShellCommands(connection.shellCommands);
    const bool fsReady =
        withBlockingSession([&]() { return m_fs.open(m_session.handle(), &sftpFailure); });

    if (m_cancelRequested.load()) {
        cleanup();
        return;
    }

    m_running = true;
    qCWarning(lcSsh) << "Connected to" << connection.host << "fs:"
                     << (fsReady ? (m_fs.backend() == FsBackend::Scp ? "scp" : "sftp") : "no");
    emit connected(initialShellId);
    emit shellOpened(initialShellId);

    if (fsReady) {
        emit remoteFsOpened(static_cast<int>(m_fs.backend()));
        emit transferResumableChanged(TransferJobStore::loadLatest(connection.id).has_value());
    } else {
        qCWarning(lcSsh) << "Remote FS unavailable:" << sftpFailure;
        emit sftpUnavailable(sftpFailure);
    }

    if (m_ioTimer == nullptr) {
        m_ioTimer = new QTimer(this);
        connect(m_ioTimer, &QTimer::timeout, this, &SshWorker::pollChannel);
    }
    m_session.resetKeepAliveClock();
    m_ioTimer->start(20);

    // Blocks until disconnectSession / fault / EOF stops the loop.
    m_ioLoop.run();
}

void SshWorker::openShell(const QUuid &shellId, int cols, int rows)
{
    if (!m_running || !m_session.isConnected()) {
        emit shellOpenFailed(shellId, tr("SSH session is not connected"));
        return;
    }
    if (shellId.isNull()) {
        emit shellOpenFailed(shellId, tr("Invalid shell id"));
        return;
    }
    if (m_shellHandlers.contains(shellId)) {
        emit shellOpenFailed(shellId, tr("Shell already open"));
        return;
    }
    if (m_shellHandlers.size() >= kMaxShells) {
        emit shellOpenFailed(shellId, tr("Maximum of %1 shells per session").arg(kMaxShells));
        return;
    }

    QString error;
    if (!openShellLocked(shellId, cols, rows, &error)) {
        emit shellOpenFailed(shellId, error.isEmpty() ? tr("Failed to open shell") : error);
        return;
    }

    emit shellOpened(shellId);
    m_ioLoop.wake();
}

ShellIoHandler::Hooks SshWorker::makeShellHooks()
{
    ShellIoHandler::Hooks hooks;
    hooks.dataReady = [this](const QUuid &id, const QByteArray &data) {
        if (!data.isEmpty()) {
            m_session.resetKeepAliveClock();
            emit dataReceived(id, data);
        }
    };
    hooks.closed = [this](const QUuid &id) {
        // Defer so we do not destroy the handler during its own onIdle.
        QMetaObject::invokeMethod(
            this, [this, id]() { retireShell(id, true); }, Qt::QueuedConnection);
    };
    hooks.failed = [this](const QUuid &id, const QString &message) {
        emit shellFailed(id, message);
    };
    hooks.againPump = [this]() {
        if (m_cancelRequested.load()) {
            return false;
        }
        return m_ioLoop.pollOnce(50);
    };
    return hooks;
}

bool SshWorker::openShellLocked(const QUuid &shellId, int cols, int rows, QString *errorOut)
{
    // PTY/shell request_exec-style opens are simpler in blocking mode; IoLoop
    // keeps the session non-blocking afterward for channel callbacks.
    return withBlockingSession([&]() -> bool {
        auto handler = std::make_unique<ShellIoHandler>(
            shellId, m_session.handle(), cols, rows, makeShellHooks());
        ShellIoHandler *raw = handler.get();
        if (!m_ioLoop.addHandler(std::move(handler), errorOut)) {
            return false;
        }
        m_shellHandlers.insert(shellId, raw);
        tryRequestAgentForwarding(raw->channel());
        return true;
    });
}

void SshWorker::tryRequestAgentForwarding(ssh_channel firstShellChannel)
{
    if (m_agentForwardRequested) {
        return;
    }
    m_agentForwardRequested = true;

    if (!m_agentForwarding || firstShellChannel == nullptr) {
        return;
    }

    if (!AgentForwardHost::isLocalAgentPresent()) {
        emit agentForwardingWarning(
            tr("Agent forwarding is enabled but no local ssh-agent is available "
               "(SSH_AUTH_SOCK). The session remains connected."));
        return;
    }

    auto *host = new AgentForwardHost(this);
    QString error;
    if (!host->start(m_session.handle(), firstShellChannel, &error)) {
        emit agentForwardingWarning(error.isEmpty() ? tr("Failed to enable agent forwarding")
                                                    : error);
        delete host;
        return;
    }
    m_agentForwardHost = host;
}

void SshWorker::closeShell(const QUuid &shellId)
{
    retireShell(shellId, true);
}

void SshWorker::retireShell(const QUuid &shellId, bool emitClosed)
{
    if (!m_shellHandlers.contains(shellId)) {
        return;
    }
    m_shellHandlers.remove(shellId);
    m_ioLoop.removeHandler(shellId.toString(QUuid::WithoutBraces));
    if (emitClosed) {
        emit shellClosed(shellId);
    }
}

void SshWorker::writeToChannel(const QUuid &shellId, const QByteArray &data)
{
    if (!m_running) {
        return;
    }

    ShellIoHandler *handler = m_shellHandlers.value(shellId, nullptr);
    if (handler == nullptr) {
        return;
    }
    handler->enqueueWrite(data);
}

void SshWorker::changePtySize(const QUuid &shellId, int cols, int rows)
{
    if (!m_running) {
        return;
    }
    ShellIoHandler *handler = m_shellHandlers.value(shellId, nullptr);
    if (handler == nullptr) {
        return;
    }
    handler->changePtySize(cols, rows);
}

void SshWorker::disconnectSession()
{
    m_cancelRequested.store(true);
    respondHostKeyTrust(false);

    if (m_ioTimer) {
        m_ioTimer->stop();
    }
    m_ioLoop.stop();

    if (m_running && m_fs.isOpen()) {
        m_fs.requestInterrupt(TransferEndReason::Interrupted, tr("Connection lost"));
    }

    const bool wasRunning = m_running;
    cleanup();

    if (wasRunning) {
        qCWarning(lcSsh) << "Session disconnected";
        emit disconnected();
    }
}

void SshWorker::onIoLoopFault(const QString &message)
{
    if (!message.isEmpty()) {
        emit errorOccurred(message);
    }
    disconnectSession();
}

void SshWorker::onIoLoopSessionEof()
{
    disconnectSession();
}

void SshWorker::requestCancel()
{
    m_cancelRequested.store(true);
    respondHostKeyTrust(false);
}

void SshWorker::respondHostKeyTrust(bool accept)
{
    QMutexLocker locker(&m_hostKeyMutex);
    m_hostKeyAccepted = accept;
    m_hostKeyAnswered = true;
    m_hostKeyCondition.wakeAll();
}

void SshWorker::listDirectory(const QString &path)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QVector<RemoteEntry> entries;
        QString error;
        if (!m_fs.listDirectoryEntries(path, &entries, &error)) {
            emit sftpError(error);
            return;
        }
        filterTransferPartials(&entries);
        emit directoryListed(path, entries);
        return;
    }

    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::ListDirectory;
    req.path = path;
    enqueueFsOp([this, req]() { startMetaHandler(req); });
}

void SshWorker::emitTransferFailure(const QString &error)
{
    if (m_fs.wasCanceled()) {
        emit sftpCanceled(error.isEmpty() ? tr("Transfer canceled — partial kept") : error);
        emit transferResumableChanged(m_fs.lastInterruptedJob().has_value());
        return;
    }
    if (m_fs.wasInterrupted()) {
        const auto job = m_fs.lastInterruptedJob();
        if (job) {
            emit sftpInterrupted(*job);
            emit transferResumableChanged(true);
            return;
        }
        emit sftpError(error.isEmpty() ? tr("Transfer interrupted") : error);
        return;
    }
    if (m_fs.lastEndReason() == TransferEndReason::HashMismatch) {
        emit sftpError(error.isEmpty() ? tr("Transfer hash mismatch") : error);
        emit transferResumableChanged(m_fs.lastInterruptedJob().has_value());
        return;
    }
    emit sftpError(error);
    emit transferResumableChanged(m_fs.lastInterruptedJob().has_value());
}

bool SshWorker::useAsyncFs() const
{
    return m_fs.backend() == FsBackend::Sftp;
}

void SshWorker::enqueueFsOp(std::function<void()> op)
{
    if (m_fsBusy) {
        m_pendingFsOps.push_back(std::move(op));
        return;
    }
    m_fsBusy = true;
    op();
}

void SshWorker::onFsHandlerFinished(const QString &handlerId)
{
    QMetaObject::invokeMethod(
        this,
        [this, handlerId]() {
            m_ioLoop.removeHandler(handlerId);
            m_fsBusy = false;
            if (m_pendingFsOps.isEmpty()) {
                return;
            }
            auto next = std::move(m_pendingFsOps.front());
            m_pendingFsOps.erase(m_pendingFsOps.begin());
            m_fsBusy = true;
            next();
        },
        Qt::QueuedConnection);
}

void SshWorker::startMetaHandler(SftpMetaIoHandler::Request request)
{
    SftpMetaIoHandler::Hooks hooks;
    hooks.listed = [this](const QString &path, const QVector<RemoteEntry> &entries) {
        QVector<RemoteEntry> filtered = entries;
        filterTransferPartials(&filtered);
        emit directoryListed(path, filtered);
    };
    hooks.resolved = [this](const QString &path, bool isDir, bool ok, const QString &error) {
        emit entryResolved(path, isDir, ok, error);
    };
    hooks.canonicalized = [this](const QString &requested, const QString &canonical) {
        emit pathCanonicalized(requested, canonical);
    };
    hooks.finished = [this](const QString &message) { emit sftpFinished(message); };
    hooks.failed = [this](const QString &message) {
        if (!message.isEmpty()) {
            emit sftpError(message);
        }
    };

    auto handler = std::make_unique<SftpMetaIoHandler>(&m_fs, std::move(request), std::move(hooks));
    const QString id = handler->id();
    handler->setCompletedHook([this, id]() { onFsHandlerFinished(id); });

    QString error;
    if (!m_ioLoop.addHandler(std::move(handler), &error)) {
        m_fsBusy = false;
        emit sftpError(error.isEmpty() ? tr("Failed to start SFTP operation") : error);
        return;
    }
    m_ioLoop.wake();
}

void SshWorker::startTransferHandler(SftpTransferIoHandler::Request request)
{
    SftpTransferIoHandler::Hooks hooks;
    hooks.finished = [this](const QString &message) {
        emit sftpFinished(message);
        emit transferResumableChanged(false);
    };
    hooks.failed = [this](const QString &error) { emitTransferFailure(error); };

    auto handler =
        std::make_unique<SftpTransferIoHandler>(&m_fs, std::move(request), std::move(hooks));
    const QString id = handler->id();
    handler->setCompletedHook([this, id]() { onFsHandlerFinished(id); });

    QString error;
    if (!m_ioLoop.addHandler(std::move(handler), &error)) {
        m_fsBusy = false;
        emit sftpError(error.isEmpty() ? tr("Failed to start transfer") : error);
        return;
    }
    m_ioLoop.wake();
}

void SshWorker::createDirectory(const QString &path)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.createDirectory(path, &error)) {
            emit sftpError(error);
            return;
        }
        emit sftpFinished(tr("Created folder: %1").arg(path));
        return;
    }

    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::CreateDirectory;
    req.path = path;
    enqueueFsOp([this, req]() { startMetaHandler(req); });
}

void SshWorker::createSymlink(const QString &target, const QString &linkPath)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.createSymlink(target, linkPath, &error)) {
            emit sftpError(error);
            return;
        }
        emit sftpFinished(tr("Created symlink: %1").arg(linkPath));
        return;
    }

    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::CreateSymlink;
    req.target = target;
    req.linkPath = linkPath;
    enqueueFsOp([this, req]() { startMetaHandler(req); });
}

void SshWorker::resolveEntry(const QString &path)
{
    if (!m_running || !m_fs.isOpen()) {
        emit entryResolved(path, false, false, tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        bool isDir = false;
        QString error;
        if (!m_fs.resolveEntry(path, &isDir, &error)) {
            emit entryResolved(path, false, false, error);
            return;
        }
        if (isDir) {
            QVector<RemoteEntry> entries;
            if (!m_fs.listDirectoryEntries(path, &entries, &error)) {
                emit entryResolved(path, true, false, error);
                return;
            }
        }
        emit entryResolved(path, isDir, true, {});
        return;
    }

    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::ResolveEntry;
    req.path = path;
    enqueueFsOp([this, req]() { startMetaHandler(req); });
}

void SshWorker::renamePath(const QString &from, const QString &to)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.renamePath(from, to, &error)) {
            emit sftpError(error);
            return;
        }
        emit sftpFinished(tr("Renamed to %1").arg(QFileInfo(to).fileName()));
        return;
    }

    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::RenamePath;
    req.from = from;
    req.to = to;
    enqueueFsOp([this, req]() { startMetaHandler(req); });
}

void SshWorker::removePath(const QString &path, bool recursive)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.removePath(path, recursive, &error)) {
            emit sftpError(error);
            return;
        }
        emit sftpFinished(tr("Deleted: %1").arg(path));
        return;
    }

    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::RemovePath;
    req.path = path;
    req.recursive = recursive;
    enqueueFsOp([this, req]() { startMetaHandler(req); });
}

void SshWorker::uploadFiles(const QStringList &localPaths, const QString &remoteDir)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.uploadFiles(localPaths, remoteDir, &error)) {
            emitTransferFailure(error);
            return;
        }
        emit sftpFinished(tr("Upload finished (%1 item(s))").arg(localPaths.size()));
        emit transferResumableChanged(false);
        return;
    }

    SftpTransferIoHandler::Request req;
    req.kind = SftpTransferIoHandler::Kind::UploadFiles;
    req.localPaths = localPaths;
    req.remoteDir = remoteDir;
    req.finishedMessage = tr("Upload finished (%1 item(s))").arg(localPaths.size());
    enqueueFsOp([this, req]() { startTransferHandler(req); });
}

void SshWorker::uploadFileTo(const QString &localPath, const QString &remotePath)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.uploadFileTo(localPath, remotePath, &error)) {
            emitTransferFailure(error);
            return;
        }
        emit sftpFinished(tr("Synced: %1").arg(QFileInfo(remotePath).fileName()));
        return;
    }

    SftpTransferIoHandler::Request req;
    req.kind = SftpTransferIoHandler::Kind::UploadFileTo;
    req.localPath = localPath;
    req.remotePath = remotePath;
    req.finishedMessage = tr("Synced: %1").arg(QFileInfo(remotePath).fileName());
    enqueueFsOp([this, req]() { startTransferHandler(req); });
}

void SshWorker::downloadPaths(const QStringList &remotePaths, const QString &localDir)
{
    downloadPaths(remotePaths, localDir, false);
}

void SshWorker::downloadPaths(const QStringList &remotePaths,
                              const QString &localDir,
                              bool followSymlinks)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.downloadPaths(remotePaths, localDir, &error, followSymlinks)) {
            emitTransferFailure(error);
            return;
        }
        emit sftpFinished(tr("Download finished (%1 item(s))").arg(remotePaths.size()));
        emit transferResumableChanged(false);
        return;
    }

    SftpTransferIoHandler::Request req;
    req.kind = SftpTransferIoHandler::Kind::DownloadPaths;
    req.remotePaths = remotePaths;
    req.localDir = localDir;
    req.followSymlinks = followSymlinks;
    req.finishedMessage = tr("Download finished (%1 item(s))").arg(remotePaths.size());
    enqueueFsOp([this, req]() { startTransferHandler(req); });
}

void SshWorker::cancelTransfer()
{
    m_fs.requestCancel();
}

void SshWorker::interruptTransfer(const QString &message)
{
    m_fs.requestInterrupt(TransferEndReason::Interrupted, message);
}

void SshWorker::resumeInterruptedTransfer()
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        QString error;
        if (!m_fs.resumeInterruptedTransfer(&error)) {
            emitTransferFailure(error);
            return;
        }
        emit sftpFinished(tr("Resumed transfer finished"));
        emit transferResumableChanged(false);
        return;
    }

    SftpTransferIoHandler::Request req;
    req.kind = SftpTransferIoHandler::Kind::ResumeInterrupted;
    req.finishedMessage = tr("Resumed transfer finished");
    enqueueFsOp([this, req]() { startTransferHandler(req); });
}

void SshWorker::discardInterruptedTransfer()
{
    QString error;
    if (!m_fs.discardInterruptedTransfer(&error)) {
        emit sftpError(error);
        return;
    }
    emit transferResumableChanged(false);
    emit sftpFinished(tr("Discarded interrupted transfer"));
}

void SshWorker::canonicalizePath(const QString &path)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    if (!useAsyncFs()) {
        const QString requested = path.isEmpty() ? QStringLiteral(".") : path;
        QString canonical;
        QString error;
        if (!m_fs.canonicalizePath(requested, canonical, &error)) {
            emit sftpError(error);
            return;
        }
        emit pathCanonicalized(requested, canonical);
        return;
    }

    SftpMetaIoHandler::Request req;
    req.op = SftpMetaIoHandler::Op::CanonicalizePath;
    req.path = path;
    enqueueFsOp([this, req]() { startMetaHandler(req); });
}

void SshWorker::pollChannel()
{
    if (!m_running) {
        return;
    }

    if (!m_session.isConnected()) {
        disconnectSession();
        return;
    }

    // Shell I/O is on SshIoLoop callbacks; timer only services tunnels / agent / keepalive.
    pollTunnels();
    if (m_agentForwardHost) {
        m_agentForwardHost->poll();
    }

    if (!m_session.isConnected()) {
        disconnectSession();
        return;
    }

    QString error;
    if (!m_session.pollKeepAlive(false, &error)) {
        emit errorOccurred(error);
        disconnectSession();
    }
}

void SshWorker::pollTunnels()
{
    QList<RemoteTunnelSession *> remotes;
    for (ITunnelSession *session : m_tunnelSessions) {
        if (auto *remote = qobject_cast<RemoteTunnelSession *>(session)) {
            remotes.append(remote);
        }
    }
    acceptRemoteForwards(m_session.handle(), remotes);

    for (ITunnelSession *session : m_tunnelSessions) {
        if (session) {
            session->poll();
        }
    }
}

void SshWorker::wireTunnelSession(ITunnelSession *session)
{
    connect(session, &ITunnelSession::statusChanged, this, &SshWorker::tunnelStatusChanged);
    connect(session, &ITunnelSession::errorOccurred, this, &SshWorker::tunnelError);
}

bool SshWorker::waitForHostKeyTrust(HostKeyPrompt reason,
                                    const QString &fingerprint,
                                    const QString &contextLabel)
{
    {
        QMutexLocker locker(&m_hostKeyMutex);
        m_hostKeyAnswered = false;
        m_hostKeyAccepted = false;
    }

    emit hostKeyPrompt(reason, fingerprint, contextLabel);

    QMutexLocker locker(&m_hostKeyMutex);
    while (!m_hostKeyAnswered) {
        m_hostKeyCondition.wait(&m_hostKeyMutex);
    }
    return m_hostKeyAccepted;
}

bool SshWorker::verifyKnownHostForSession(ssh_session session, const QString &contextLabel)
{
    return SshKnownHosts::verify(
        session,
        contextLabel,
        [this](SshKnownHosts::Disposition disposition,
               const QString &fingerprint,
               const QString &label) {
            HostKeyPrompt reason = HostKeyPrompt::Unknown;
            switch (disposition) {
            case SshKnownHosts::Disposition::Changed:
                reason = HostKeyPrompt::Changed;
                break;
            case SshKnownHosts::Disposition::Other:
                reason = HostKeyPrompt::Other;
                break;
            case SshKnownHosts::Disposition::Unknown:
                reason = HostKeyPrompt::Unknown;
                break;
            }
            return waitForHostKeyTrust(reason, fingerprint, label);
        },
        [this](const QString &message) { emit errorOccurred(message); });
}

void SshWorker::cleanup()
{
    m_running = false;

    if (m_ioTimer) {
        m_ioTimer->stop();
    }
    m_ioLoop.stop();

    stopAllTunnels();

    // Close agent bridges while the session is still alive, but keep AgentForwardHost
    // (and its ssh_callbacks_struct) until after ssh_free — libssh does not copy callbacks.
    if (m_agentForwardHost) {
        m_agentForwardHost->stop();
    }
    m_agentForwardRequested = false;
    m_agentForwarding = false;

    m_fs.close();

    const QList<QUuid> shellIds = m_shellHandlers.keys();
    for (const QUuid &id : shellIds) {
        m_shellHandlers.remove(id);
        m_ioLoop.removeHandler(id.toString(QUuid::WithoutBraces));
    }
    m_ioLoop.detachSession();
    m_session.cleanup();

    delete m_agentForwardHost;
    m_agentForwardHost = nullptr;
}

void SshWorker::startTunnel(const TunnelDefinition &def)
{
    if (!m_running || !m_session.isConnected()) {
        emit tunnelError(def.id, tr("SSH session is not connected"));
        emit tunnelStatusChanged(
            def.id, QStringLiteral("Error"), tr("SSH session is not connected"));
        return;
    }

    if (def.id.isNull() || !def.isValid()) {
        const QString message = def.validationError().isEmpty() ? tr("Invalid tunnel definition")
                                                                : def.validationError();
        emit tunnelError(def.id, message);
        emit tunnelStatusChanged(def.id, QStringLiteral("Error"), message);
        return;
    }

    if (m_tunnelSessions.contains(def.id)) {
        stopTunnel(def.id);
    }

    emit tunnelStatusChanged(def.id, QStringLiteral("Starting"), QString());

    ITunnelSession *session = createTunnelSession(def, m_session.handle(), this);
    if (session == nullptr) {
        const QString message = tr("Unsupported tunnel type");
        emit tunnelError(def.id, message);
        emit tunnelStatusChanged(def.id, QStringLiteral("Error"), message);
        return;
    }

    wireTunnelSession(session);
    if (!session->start()) {
        session->deleteLater();
        return;
    }

    m_tunnelSessions.insert(def.id, session);
    emit tunnelStatusChanged(def.id, QStringLiteral("Listening"), QString());
}

void SshWorker::stopTunnel(const QUuid &tunnelId)
{
    ITunnelSession *session = m_tunnelSessions.take(tunnelId);
    if (session == nullptr) {
        emit tunnelStatusChanged(tunnelId, QStringLiteral("Off"), QString());
        return;
    }

    session->stop(true);
    session->deleteLater();
}

void SshWorker::stopAllTunnels()
{
    const QList<QUuid> ids = m_tunnelSessions.keys();
    for (const QUuid &id : ids) {
        ITunnelSession *session = m_tunnelSessions.take(id);
        if (session) {
            session->stop(true);
            session->deleteLater();
        }
    }
}

void SshWorker::pumpIoDuringBlockingOp()
{
    if (!m_running || !m_session.isConnected()) {
        return;
    }

    // Keep interactive shell / tunnels responsive while a one-shot exec blocks.
    // Shell data arrives via channel callbacks during pollOnce.
    pollTunnels();
    if (m_agentForwardHost) {
        m_agentForwardHost->poll();
    }
    m_ioLoop.pollOnce(0);

    // Drain queued writes / other worker slots without starting nested user input.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void SshWorker::execCommand(QStringView requestId, const QString &command)
{
    const QString id(requestId);
    if (!m_running || !m_session.isConnected() || m_session.handle() == nullptr) {
        emit commandFinished(id, -1, {}, {}, tr("SSH session is not connected"));
        return;
    }
    if (command.trimmed().isEmpty()) {
        emit commandFinished(id, -1, {}, {}, tr("Empty remote command"));
        return;
    }

    if (m_execBusy) {
        m_pendingExecCommands.push_back(PendingExecCommand{id, command});
        return;
    }

    runExecCommand(id, command);

    while (!m_pendingExecCommands.isEmpty()) {
        if (!m_running || !m_session.isConnected() || m_session.handle() == nullptr) {
            const QVector<PendingExecCommand> dropped = std::move(m_pendingExecCommands);
            m_pendingExecCommands.clear();
            for (const PendingExecCommand &pending : dropped) {
                emit commandFinished(
                    pending.requestId, -1, {}, {}, tr("SSH session is not connected"));
            }
            return;
        }
        const PendingExecCommand pending = m_pendingExecCommands.takeFirst();
        runExecCommand(pending.requestId, pending.command);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void SshWorker::runExecCommand(const QString &id, const QString &command)
{
    m_execBusy = true;

    ShellExecRunner runner(m_session.handle());
    runner.setPump([this]() { pumpIoDuringBlockingOp(); });
    ShellExecRunner::Result result;
    QString error;
    const bool ok = runner.run(command, &result, &error);

    m_execBusy = false;

    if (!ok) {
        emit commandFinished(id,
                             result.exitStatus,
                             result.stdoutBytes,
                             result.stderrBytes,
                             error.isEmpty() ? result.errorMessage : error);
        return;
    }

    emit commandFinished(id, result.exitStatus, result.stdoutBytes, result.stderrBytes, QString());
}
