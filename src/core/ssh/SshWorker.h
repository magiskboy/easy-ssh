/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"
#include "core/fs/FsRemote.h"
#include "core/fs/SftpTypes.h"
#include "core/fs/TransferTypes.h"
#include "core/shell/SshShell.h"
#include "core/ssh/SshKnownHosts.h"
#include "core/ssh/SshSession.h"
#include "core/tunnel/ITunnelSession.h"
#include "core/tunnel/Tunnel.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QUuid>
#include <QVector>
#include <QWaitCondition>

#include <atomic>

#if defined(LIBSSH_VERSION_INT) && (LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 11, 0))
#error "easy-ssh requires libssh >= 0.11 for ProxyJump (SSH_OPTIONS_PROXYJUMP)"
#endif

/// Worker-thread SSH I/O for one transport (SshSession). Transport teardown via
/// disconnectSession() — does not destroy the domain Session object.
class SshWorker final : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxShells = 8;

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
    /// Establish transport + SFTP, open initial shell, start poll timer.
    void connectToHost(const Connection &connection,
                       const SessionCredentials &credentials,
                       const QUuid &initialShellId,
                       int cols = 80,
                       int rows = 24);
    void openShell(const QUuid &shellId, int cols = 80, int rows = 24);
    void closeShell(const QUuid &shellId);
    void writeToChannel(const QUuid &shellId, const QByteArray &data);
    void changePtySize(const QUuid &shellId, int cols, int rows);
    /// Tear down transport (all shells, SFTP, tunnels, SshSession). Not domain Session destroy.
    void disconnectSession();
    /// Thread-safe: abort in-flight connect / host-key wait. Safe from any thread.
    void requestCancel();
    void respondHostKeyTrust(bool accept);

    void listDirectory(const QString &path);
    void createDirectory(const QString &path);
    void createSymlink(const QString &target, const QString &linkPath);
    void resolveEntry(const QString &path);
    void renamePath(const QString &from, const QString &to);
    void removePath(const QString &path, bool recursive);
    void uploadFiles(const QStringList &localPaths, const QString &remoteDir);
    void uploadFileTo(const QString &localPath, const QString &remotePath);
    void downloadPaths(const QStringList &remotePaths, const QString &localDir);
    void
    downloadPaths(const QStringList &remotePaths, const QString &localDir, bool followSymlinks);
    void canonicalizePath(const QString &path);
    void cancelTransfer();
    void resumeInterruptedTransfer();
    void discardInterruptedTransfer();
    void interruptTransfer(const QString &message = {});

    void startTunnel(const TunnelDefinition &def);
    void stopTunnel(const QUuid &tunnelId);
    void stopAllTunnels();

    /// One-shot remote exec (no PTY). @p requestId is echoed in commandFinished.
    void execCommand(QStringView requestId, const QString &command);

signals:
    void connected(const QUuid &initialShellId);
    void dataReceived(const QUuid &shellId, const QByteArray &data);
    void shellOpened(const QUuid &shellId);
    void shellClosed(const QUuid &shellId);
    void shellFailed(const QUuid &shellId, const QString &message);
    void shellOpenFailed(const QUuid &shellId, const QString &message);
    void hostKeyPrompt(SshWorker::HostKeyPrompt reason,
                       const QString &fingerprintSha256,
                       const QString &contextLabel);
    void errorOccurred(const QString &message);
    void disconnected();

    void directoryListed(const QString &path, const QVector<RemoteEntry> &entries);
    void entryResolved(const QString &path, bool isDir, bool ok, const QString &error);
    void pathCanonicalized(const QString &requested, const QString &canonical);
    void sftpFinished(const QString &message);
    void sftpError(const QString &message);
    void sftpCanceled(const QString &message);
    void sftpInterrupted(const TransferJob &job);
    void sftpUnavailable(const QString &message);
    void sftpProgress(qint64 bytesDone, qint64 bytesTotal, const QString &currentName);
    void transferResumableChanged(bool resumable);
    /// Emitted after connect when remote FS opened (Sftp or Scp).
    void remoteFsOpened(int backend);

    void tunnelStatusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void tunnelError(const QUuid &tunnelId, const QString &message);
    /// Soft warning (e.g. ForwardAgent ON but no local agent). Must not tear down the session.
    void agentForwardingWarning(const QString &message);

    void commandFinished(const QString &requestId,
                         int exitStatus,
                         const QByteArray &stdoutBytes,
                         const QByteArray &stderrBytes,
                         const QString &errorMessage);

private slots:
    void pollChannel();

private:
    bool waitForHostKeyTrust(HostKeyPrompt reason,
                             const QString &fingerprint,
                             const QString &contextLabel);
    bool verifyKnownHostForSession(ssh_session session, const QString &contextLabel);
    void cleanup();
    void pollTunnels();
    void pumpIoDuringBlockingOp();
    void runExecCommand(const QString &requestId, const QString &command);
    void wireTunnelSession(ITunnelSession *session);
    void retireShell(const QUuid &shellId, bool emitClosed);
    bool openShellLocked(const QUuid &shellId, int cols, int rows, QString *errorOut);
    void tryRequestAgentForwarding(SshShell *shell);
    void emitTransferFailure(const QString &error);

    SshSession m_session;
    QHash<QUuid, SshShell *> m_shells;
    FsRemote m_fs;
    QHash<QUuid, ITunnelSession *> m_tunnelSessions;
    class AgentForwardHost *m_agentForwardHost = nullptr;
    bool m_agentForwarding = false;
    bool m_agentForwardRequested = false;
    class QTimer *m_ioTimer = nullptr;
    bool m_running = false;
    bool m_execBusy = false;
    struct PendingExecCommand
    {
        QString requestId;
        QString command;
    };
    QVector<PendingExecCommand> m_pendingExecCommands;
    std::atomic<bool> m_cancelRequested{false};

    QMutex m_hostKeyMutex;
    QWaitCondition m_hostKeyCondition;
    bool m_hostKeyAnswered = false;
    bool m_hostKeyAccepted = false;
};
