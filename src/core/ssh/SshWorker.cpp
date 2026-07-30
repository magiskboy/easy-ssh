#include "SshWorker.h"

#include "core/util/Logging.h"

#include <QDateTime>
#include <QFileInfo>
#include <QTimer>

SshWorker::SshWorker(QObject *parent) : QObject(parent)
{
    m_sftp.setProgressCallback([this](qint64 done, qint64 total, const QString &name) {
        emit sftpProgress(done, total, name);
    });

    m_tunnels = new SshTunnelManager(this);
    connect(m_tunnels, &SshTunnelManager::tunnelStatusChanged, this, &SshWorker::tunnelStatusChanged);
    connect(m_tunnels, &SshTunnelManager::tunnelError, this, &SshWorker::tunnelError);

    m_session.setHostKeyVerifier(
        [this](ssh_session session, const QString &contextLabel) {
            return verifyKnownHostForSession(session, contextLabel);
        });
}

SshWorker::~SshWorker()
{
    cleanup();
}

void SshWorker::connectToHost(const Connection &connection,
                              const SessionCredentials &credentials,
                              int cols,
                              int rows)
{
    if (m_running) {
        emit errorOccurred(tr("Session already connected"));
        return;
    }

    cleanup();

    QString error;
    if (!m_session.establish(connection, credentials, cols, rows, &error)) {
        if (!error.isEmpty()) {
            emit errorOccurred(error);
        }
        return;
    }

    QString sftpFailure;
    const bool sftpReady = m_sftp.open(m_session.handle(), &sftpFailure);

    m_running = true;
    m_tunnels->setSession(m_session.handle());
    qCWarning(lcSsh) << "Connected to" << connection.host << "sftp:" << (sftpReady ? "yes" : "no");
    emit connected();

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

void SshWorker::writeToChannel(const QByteArray &data)
{
    if (!m_running) {
        return;
    }

    QString error;
    if (!m_session.writeToChannel(data, &error)) {
        if (!error.isEmpty()) {
            emit errorOccurred(error);
        }
        disconnectSession();
    }
}

void SshWorker::changePtySize(int cols, int rows)
{
    if (!m_running) {
        return;
    }
    m_session.changePtySize(cols, rows);
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
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QVector<RemoteEntry> entries;
    QString error;
    if (!m_sftp.listDirectoryEntries(path, &entries, &error)) {
        emit sftpError(error);
        return;
    }

    emit directoryListed(path, entries);
}

void SshWorker::createDirectory(const QString &path)
{
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_sftp.createDirectory(path, &error)) {
        emit sftpError(error);
        return;
    }

    emit sftpFinished(tr("Created folder: %1").arg(path));
}

void SshWorker::renamePath(const QString &from, const QString &to)
{
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_sftp.renamePath(from, to, &error)) {
        emit sftpError(error);
        return;
    }

    emit sftpFinished(tr("Renamed to %1").arg(QFileInfo(to).fileName()));
}

void SshWorker::removePath(const QString &path, bool recursive)
{
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_sftp.removePath(path, recursive, &error)) {
        emit sftpError(error);
        return;
    }

    emit sftpFinished(tr("Deleted: %1").arg(path));
}

void SshWorker::uploadFiles(const QStringList &localPaths, const QString &remoteDir)
{
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_sftp.uploadFiles(localPaths, remoteDir, &error)) {
        if (m_sftp.wasCanceled()) {
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
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_sftp.uploadFileTo(localPath, remotePath, &error)) {
        if (m_sftp.wasCanceled()) {
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
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QString error;
    if (!m_sftp.downloadPaths(remotePaths, localDir, &error)) {
        if (m_sftp.wasCanceled()) {
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
    m_sftp.requestCancel();
}

void SshWorker::canonicalizePath(const QString &path)
{
    if (!m_running || !m_sftp.isOpen()) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    const QString requested = path.isEmpty() ? QStringLiteral(".") : path;
    QString canonical;
    QString error;
    if (!m_sftp.canonicalizePath(requested, &canonical, &error)) {
        emit sftpError(error);
        return;
    }

    emit pathCanonicalized(requested, canonical);
}

void SshWorker::pollChannel()
{
    if (!m_running || !m_session.isConnected()) {
        return;
    }

    m_tunnels->poll();

    QByteArray data;
    QString error;
    const SshSession::ShellPollStatus status = m_session.pollShell(&data, &error);
    switch (status) {
    case SshSession::ShellPollStatus::Data:
        if (!data.isEmpty()) {
            emit dataReceived(data);
        }
        if (!m_session.pollKeepAlive(true, &error)) {
            emit errorOccurred(error);
            disconnectSession();
        }
        break;
    case SshSession::ShellPollStatus::Idle:
        if (!m_session.pollKeepAlive(false, &error)) {
            emit errorOccurred(error);
            disconnectSession();
        }
        break;
    case SshSession::ShellPollStatus::Error:
        emit errorOccurred(error);
        disconnectSession();
        break;
    case SshSession::ShellPollStatus::Disconnected:
        disconnectSession();
        break;
    }
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

    m_tunnels->stopAll();
    m_tunnels->setSession(nullptr);

    m_sftp.close();
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
    m_tunnels->startTunnel(def);
}

void SshWorker::stopTunnel(const QUuid &tunnelId)
{
    m_tunnels->stopTunnel(tunnelId);
}

void SshWorker::stopAllTunnels()
{
    m_tunnels->stopAll();
}
