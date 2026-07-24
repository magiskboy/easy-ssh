#pragma once

#include "Connection.h"
#include "SftpTypes.h"
#include "Tunnel.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <QWaitCondition>

#include <atomic>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

class QTcpServer;
class QTcpSocket;

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
                       const QString &secret,
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
    void hostKeyPrompt(SshWorker::HostKeyPrompt reason, const QString &fingerprintSha256);
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
    void onLocalTunnelNewConnection();
    void onBridgeSocketReadyRead();
    void onBridgeSocketDisconnected();

private:
    struct TunnelBridge
    {
        QUuid tunnelId;
        ssh_channel channel = nullptr;
        QTcpSocket *socket = nullptr;
        bool closing = false;
    };

    struct ActiveTunnel
    {
        TunnelDefinition def;
        QTcpServer *server = nullptr;
        bool remoteListening = false;
        QList<TunnelBridge *> bridges;
    };

    bool verifyKnownHost();
    bool promptHostKeyAndUpdate(HostKeyPrompt reason, bool removeExistingEntries);
    bool removeKnownHostsEntriesForSession();
    QString knownHostsFilePath() const;
    bool authenticate(const Connection &connection, QString &secret);
    bool authenticatePassword(const QString &password);
    bool authenticateKeyboardInteractive(const QString &password);
    bool authenticatePrivateKey(const QString &keyPath, const QString &passphrase);
    bool openShell();
    bool openSftp(QString *failureMessage = nullptr);
    void cleanup();
    QString sessionError() const;
    QString sftpErrorMessage() const;
    static QString fingerprintOf(ssh_session session);
    static bool knownHostsLineMatchesHost(const QString &hostField, const QString &host, int port);
    static QString formatPermissions(uint32_t permissions, uint8_t type);
    static QString localIoErrorMessage(const QString &qtErrorString);

    void beginTransfer(qint64 bytesTotal);
    void endTransfer();
    bool transferCanceled(QString *error) const;
    void noteTransferProgress(qint64 bytesDelta, const QString &currentName);
    qint64 computeLocalBytes(const QStringList &localPaths) const;
    qint64 computeLocalPathBytes(const QString &localPath) const;
    qint64 computeRemoteBytes(const QStringList &remotePaths);
    qint64 computeRemotePathBytes(const QString &remotePath, bool isDir);

    bool
    listDirectoryEntries(const QString &path, QVector<RemoteEntry> *outEntries, QString *error);
    bool removePathRecursive(const QString &path, QString *error);
    bool uploadPathRecursive(const QString &localPath, const QString &remotePath, QString *error);
    bool downloadPathRecursive(const QString &remotePath,
                               const QString &localPath,
                               bool isDir,
                               QString *error);
    bool uploadFile(const QString &localPath, const QString &remotePath, QString *error);
    bool downloadFile(const QString &remotePath, const QString &localPath, QString *error);
    bool isRemoteDirectory(const QString &path, bool *isDir, QString *error);

    bool startLocalTunnel(ActiveTunnel *tunnel);
    bool startRemoteTunnel(ActiveTunnel *tunnel);
    void acceptRemoteForwards();
    bool openLocalForwardBridge(ActiveTunnel *tunnel, QTcpSocket *socket);
    bool openRemoteForwardBridge(ActiveTunnel *tunnel, ssh_channel channel);
    void pollTunnelBridges();
    void closeBridge(TunnelBridge *bridge);
    void destroyTunnel(ActiveTunnel *tunnel, bool emitOff);
    ActiveTunnel *tunnelForServer(QTcpServer *server);
    TunnelBridge *bridgeForSocket(QTcpSocket *socket);
    TunnelBridge *bridgeForChannel(ssh_channel channel);

    ssh_session m_session = nullptr;
    ssh_channel m_channel = nullptr;
    sftp_session m_sftp = nullptr;
    class QTimer *m_ioTimer = nullptr;
    bool m_running = false;
    int m_ptyCols = 80;
    int m_ptyRows = 24;

    std::atomic_bool m_transferCancel{false};
    qint64 m_progressBytesDone = 0;
    qint64 m_progressBytesTotal = -1;
    qint64 m_progressLastEmitBytes = 0;
    qint64 m_progressLastEmitMs = 0;

    QMutex m_hostKeyMutex;
    QWaitCondition m_hostKeyCondition;
    bool m_hostKeyAnswered = false;
    bool m_hostKeyAccepted = false;

    QHash<QUuid, ActiveTunnel *> m_tunnels;
};
