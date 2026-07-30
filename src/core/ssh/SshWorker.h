#pragma once

#include "core/connection/Connection.h"
#include "SftpClient.h"
#include "SshKnownHosts.h"
#include "SshSession.h"
#include "SshTunnelManager.h"
#include "SftpTypes.h"
#include "core/tunnel/Tunnel.h"

#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <QWaitCondition>

#if defined(LIBSSH_VERSION_INT) && (LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 11, 0))
#error "easy-ssh requires libssh >= 0.11 for ProxyJump (SSH_OPTIONS_PROXYJUMP)"
#endif

class SshWorker final : public QObject
{
    Q_OBJECT

public:
    enum class HostKeyPrompt
    {
        Unknown,
        Changed,
        Other,
    };
    Q_ENUM(HostKeyPrompt)

    explicit SshWorker(QObject *parent = nullptr);
    ~SshWorker() override;

public slots:
    void connectToHost(const Connection &connection,
                       const SessionCredentials &credentials,
                       int cols = 80,
                       int rows = 24);
    void writeToChannel(const QByteArray &data);
    void changePtySize(int cols, int rows);
    void disconnectSession();
    void respondHostKeyTrust(bool accept);

    void listDirectory(const QString &path);
    void createDirectory(const QString &path);
    void renamePath(const QString &from, const QString &to);
    void removePath(const QString &path, bool recursive);
    void uploadFiles(const QStringList &localPaths, const QString &remoteDir);
    void uploadFileTo(const QString &localPath, const QString &remotePath);
    void downloadPaths(const QStringList &remotePaths, const QString &localDir);
    void canonicalizePath(const QString &path);
    void cancelTransfer();

    void startTunnel(const TunnelDefinition &def);
    void stopTunnel(const QUuid &tunnelId);
    void stopAllTunnels();

signals:
    void connected();
    void dataReceived(const QByteArray &data);
    void hostKeyPrompt(SshWorker::HostKeyPrompt reason,
                       const QString &fingerprintSha256,
                       const QString &contextLabel);
    void errorOccurred(const QString &message);
    void disconnected();

    void directoryListed(const QString &path, const QVector<RemoteEntry> &entries);
    void pathCanonicalized(const QString &requested, const QString &canonical);
    void sftpFinished(const QString &message);
    void sftpError(const QString &message);
    void sftpCanceled(const QString &message);
    void sftpUnavailable(const QString &message);
    void sftpProgress(qint64 bytesDone, qint64 bytesTotal, const QString &currentName);

    void tunnelStatusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void tunnelError(const QUuid &tunnelId, const QString &message);

private slots:
    void pollChannel();

private:
    bool waitForHostKeyTrust(HostKeyPrompt reason,
                             const QString &fingerprint,
                             const QString &contextLabel);
    bool verifyKnownHostForSession(ssh_session session, const QString &contextLabel);
    void cleanup();

    SshSession m_session;
    SftpClient m_sftp;
    SshTunnelManager *m_tunnels = nullptr;
    class QTimer *m_ioTimer = nullptr;
    bool m_running = false;

    QMutex m_hostKeyMutex;
    QWaitCondition m_hostKeyCondition;
    bool m_hostKeyAnswered = false;
    bool m_hostKeyAccepted = false;
};
