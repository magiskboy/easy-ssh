#pragma once

#include "core/connection/Connection.h"
#include "SftpTypes.h"
#include "core/tunnel/Tunnel.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <QWaitCondition>

#include <atomic>
#include <vector>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/sftp.h>

#if defined(LIBSSH_VERSION_INT) && (LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 11, 0))
#error "easy-ssh requires libssh >= 0.11 for ProxyJump (SSH_OPTIONS_PROXYJUMP)"
#endif

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

    struct JumpHopContext
    {
        SshWorker *worker = nullptr;
        int hopIndex = 0;
    };

    bool verifyKnownHost();
    bool verifyKnownHostForSession(ssh_session session, const QString &contextLabel);
    bool applyConnectionOptions(ssh_session session, const Connection &connection);
    void applyAdvancedOptions(ssh_session session, const Connection &connection);
    void registerJumpCallbacks(ssh_session session, const Connection &connection);
    bool connectSessionWithFallback(const Connection &connection, QString *errorOut);
    void applyWindowsAlgorithmFallback(ssh_session session);
    void logSessionOptions(ssh_session session, const char *stage) const;
    bool promptHostKeyAndUpdate(HostKeyPrompt reason, bool removeExistingEntries);
    bool promptHostKeyAndUpdateForSession(ssh_session session,
                                          HostKeyPrompt reason,
                                          bool removeExistingEntries,
                                          const QString &contextLabel);
    bool removeKnownHostsEntriesForSession(ssh_session session);
    bool authenticateSession(ssh_session session, const Connection &profile, QString secret);
    bool authenticate(const Connection &connection, QString &secret);
    bool authenticatePassword(ssh_session session, const QString &password);
    bool authenticateKeyboardInteractive(ssh_session session, const QString &password);
    bool authenticateWithAgent(ssh_session session);
    bool
    authenticatePrivateKey(ssh_session session, const QString &keyPath, const QString &passphrase);
    bool authenticatePublicKeyAuto(ssh_session session, const QString &passphrase);
    bool openShell();
    bool openSftp(QString *failureMessage = nullptr);
    void cleanup();
    void pollKeepAlive(bool hadChannelActivity);
    QString sessionError() const;
    QString sessionErrorOf(ssh_session session) const;
    QString sftpErrorMessage() const;
    static QString fingerprintOf(ssh_session session);
    static bool knownHostsLineMatchesHost(const QString &hostField, const QString &host, int port);
    static QString formatPermissions(uint32_t permissions, uint8_t type);
    static QString localIoErrorMessage(const QString &qtErrorString);

    int handleJumpBeforeConnection(ssh_session session, int hopIndex);
    int handleJumpVerifyKnownHost(ssh_session session, int hopIndex);
    int handleJumpAuthenticate(ssh_session session, int hopIndex);

    static int jumpBeforeConnectionCb(ssh_session session, void *userdata);
    static int jumpVerifyKnownHostCb(ssh_session session, void *userdata);
    static int jumpAuthenticateCb(ssh_session session, void *userdata);

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

    Connection m_connectConnection;
    SessionCredentials m_connectCredentials;
    std::vector<JumpHopContext> m_jumpContexts;
    std::vector<ssh_jump_callbacks_struct> m_jumpCallbacks;

    int m_keepAliveIntervalSec = 0;
    int m_keepAliveCountMax = 3;
    qint64 m_lastKeepAliveMs = 0;
    int m_keepAliveMissed = 0;

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
