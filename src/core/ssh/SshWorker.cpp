// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SshWorker.h"

#include "core/tunnel/RemoteTunnelSession.h"
#include "core/util/Logging.h"

#include <QFileInfo>
#include <QTimer>

#include <libssh/libssh.h>

SshWorker::SshWorker(QObject *parent) : QObject(parent)
{
    m_fs.setProgressCallback([this](qint64 done, qint64 total, const QString &name) {
        emit sftpProgress(done, total, name);
    });

    m_session.setHostKeyVerifier([this](ssh_session session, const QString &contextLabel) {
        return verifyKnownHostForSession(session, contextLabel);
    });
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

    cleanup();

    QString error;
    if (!m_session.establish(connection, credentials, &error)) {
        if (!error.isEmpty()) {
            emit errorOccurred(error);
        }
        return;
    }

    if (!openShellLocked(initialShellId, cols, rows, &error)) {
        if (!error.isEmpty()) {
            emit errorOccurred(error);
        }
        m_session.cleanup();
        return;
    }

    QString sftpFailure;
    const bool sftpReady = m_fs.open(m_session.handle(), &sftpFailure);

    m_running = true;
    qCWarning(lcSsh) << "Connected to" << connection.host << "sftp:" << (sftpReady ? "yes" : "no");
    emit connected(initialShellId);
    emit shellOpened(initialShellId);

    if (!sftpReady) {
        qCWarning(lcSsh) << "SFTP unavailable:" << sftpFailure;
        emit sftpUnavailable(sftpFailure);
    }

    if (m_ioTimer == nullptr) {
        m_ioTimer = new QTimer(this);
        connect(m_ioTimer, &QTimer::timeout, this, &SshWorker::pollChannel);
    }
    m_session.resetKeepAliveClock();
    m_ioTimer->start(20);
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
    if (m_shells.contains(shellId)) {
        emit shellOpenFailed(shellId, tr("Shell already open"));
        return;
    }
    if (m_shells.size() >= kMaxShells) {
        emit shellOpenFailed(shellId, tr("Maximum of %1 shells per session").arg(kMaxShells));
        return;
    }

    QString error;
    if (!openShellLocked(shellId, cols, rows, &error)) {
        emit shellOpenFailed(shellId, error.isEmpty() ? tr("Failed to open shell") : error);
        return;
    }

    emit shellOpened(shellId);
}

bool SshWorker::openShellLocked(const QUuid &shellId, int cols, int rows, QString *errorOut)
{
    auto *shell = new SshShell();
    if (!shell->open(m_session.handle(), cols, rows, errorOut)) {
        delete shell;
        return false;
    }
    m_shells.insert(shellId, shell);
    return true;
}

void SshWorker::closeShell(const QUuid &shellId)
{
    retireShell(shellId, true);
}

void SshWorker::retireShell(const QUuid &shellId, bool emitClosed)
{
    SshShell *shell = m_shells.take(shellId);
    if (shell == nullptr) {
        return;
    }
    shell->cleanup();
    delete shell;
    if (emitClosed) {
        emit shellClosed(shellId);
    }
}

void SshWorker::writeToChannel(const QUuid &shellId, const QByteArray &data)
{
    if (!m_running) {
        return;
    }

    SshShell *shell = m_shells.value(shellId, nullptr);
    if (shell == nullptr) {
        return;
    }

    QString error;
    if (!shell->write(data, &error)) {
        if (!m_session.isConnected() ||
            (m_session.handle() && !ssh_is_connected(m_session.handle()))) {
            if (!error.isEmpty()) {
                emit errorOccurred(error);
            }
            disconnectSession();
            return;
        }
        if (!error.isEmpty()) {
            emit shellFailed(shellId, error);
        }
        retireShell(shellId, true);
    }
}

void SshWorker::changePtySize(const QUuid &shellId, int cols, int rows)
{
    if (!m_running) {
        return;
    }
    SshShell *shell = m_shells.value(shellId, nullptr);
    if (shell == nullptr) {
        return;
    }
    shell->changePtySize(cols, rows);
}

void SshWorker::disconnectSession()
{
    if (m_ioTimer) {
        m_ioTimer->stop();
    }

    const bool wasRunning = m_running;
    cleanup();

    if (wasRunning) {
        qCWarning(lcSsh) << "Session disconnected";
        emit disconnected();
    }
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

    QVector<RemoteEntry> entries;
    QString error;
    if (!m_fs.listDirectoryEntries(path, &entries, &error)) {
        emit sftpError(error);
        return;
    }

    emit directoryListed(path, entries);
}

void SshWorker::createDirectory(const QString &path)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_fs.createDirectory(path, &error)) {
        emit sftpError(error);
        return;
    }

    emit sftpFinished(tr("Created folder: %1").arg(path));
}

void SshWorker::renamePath(const QString &from, const QString &to)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_fs.renamePath(from, to, &error)) {
        emit sftpError(error);
        return;
    }

    emit sftpFinished(tr("Renamed to %1").arg(QFileInfo(to).fileName()));
}

void SshWorker::removePath(const QString &path, bool recursive)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_fs.removePath(path, recursive, &error)) {
        emit sftpError(error);
        return;
    }

    emit sftpFinished(tr("Deleted: %1").arg(path));
}

void SshWorker::uploadFiles(const QStringList &localPaths, const QString &remoteDir)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_fs.uploadFiles(localPaths, remoteDir, &error)) {
        if (m_fs.wasCanceled()) {
            emit sftpCanceled(error);
        } else {
            emit sftpError(error);
        }
        return;
    }

    emit sftpFinished(tr("Upload finished (%1 item(s))").arg(localPaths.size()));
}

void SshWorker::uploadFileTo(const QString &localPath, const QString &remotePath)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_fs.uploadFileTo(localPath, remotePath, &error)) {
        if (m_fs.wasCanceled()) {
            emit sftpCanceled(error);
        } else {
            emit sftpError(error);
        }
        return;
    }

    emit sftpFinished(tr("Synced: %1").arg(QFileInfo(remotePath).fileName()));
}

void SshWorker::downloadPaths(const QStringList &remotePaths, const QString &localDir)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_fs.downloadPaths(remotePaths, localDir, &error)) {
        if (m_fs.wasCanceled()) {
            emit sftpCanceled(error);
        } else {
            emit sftpError(error);
        }
        return;
    }

    emit sftpFinished(tr("Download finished (%1 item(s))").arg(remotePaths.size()));
}

void SshWorker::cancelTransfer()
{
    m_fs.requestCancel();
}

void SshWorker::canonicalizePath(const QString &path)
{
    if (!m_running || !m_fs.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    const QString requested = path.isEmpty() ? QStringLiteral(".") : path;
    QString canonical;
    QString error;
    if (!m_fs.canonicalizePath(requested, canonical, &error)) {
        emit sftpError(error);
        return;
    }

    emit pathCanonicalized(requested, canonical);
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

    pollTunnels();

    bool hadActivity = false;
    const QList<QUuid> shellIds = m_shells.keys();
    for (const QUuid &shellId : shellIds) {
        SshShell *shell = m_shells.value(shellId, nullptr);
        if (shell == nullptr) {
            continue;
        }

        QByteArray data;
        QString error;
        const SshShell::PollStatus status = shell->poll(&data, &error);
        switch (status) {
        case SshShell::PollStatus::Data:
            if (!data.isEmpty()) {
                emit dataReceived(shellId, data);
            }
            hadActivity = true;
            break;
        case SshShell::PollStatus::Idle:
            break;
        case SshShell::PollStatus::ChannelClosed:
            retireShell(shellId, true);
            break;
        case SshShell::PollStatus::Error:
            if (!m_session.isConnected() ||
                (m_session.handle() && !ssh_is_connected(m_session.handle()))) {
                if (!error.isEmpty()) {
                    emit errorOccurred(error);
                }
                disconnectSession();
                return;
            }
            emit shellFailed(shellId, error.isEmpty() ? tr("Shell read error") : error);
            retireShell(shellId, true);
            break;
        }
    }

    if (!m_session.isConnected()) {
        disconnectSession();
        return;
    }

    QString error;
    if (!m_session.pollKeepAlive(hadActivity, &error)) {
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

    stopAllTunnels();
    m_fs.close();
    const QList<QUuid> shellIds = m_shells.keys();
    for (const QUuid &id : shellIds) {
        SshShell *shell = m_shells.take(id);
        if (shell) {
            shell->cleanup();
            delete shell;
        }
    }
    m_session.cleanup();
}

void SshWorker::startTunnel(const TunnelDefinition &def)
{
    if (!m_running || !m_session.isConnected()) {
        emit tunnelError(def.id, tr("SSH session is not connected"));
        emit tunnelStatusChanged(
            def.id, QStringLiteral("Error"), tr("SSH session is not connected"));
        return;
    }

    if (def.id.isNull() ||
        (def.type != TunnelType::Dynamic && (def.localPort == 0 || def.remotePort == 0))) {
        emit tunnelError(def.id, tr("Invalid tunnel definition"));
        emit tunnelStatusChanged(def.id, QStringLiteral("Error"), tr("Invalid tunnel definition"));
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
