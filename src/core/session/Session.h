/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"
#include "core/fs/SftpTypes.h"
#include "core/fs/TransferTypes.h"
#include "core/session/SessionTypes.h"
#include "core/ssh/SshWorker.h"
#include "core/tunnel/Tunnel.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

class QThread;

/// Domain runtime for one Connection: owns SshWorker thread, not SshSession directly.
class Session final : public QObject
{
    Q_OBJECT

public:
    explicit Session(const Connection &connection,
                     const SessionCredentials &credentials,
                     QObject *parent = nullptr);
    ~Session() override;

    QUuid id() const { return m_connection.id; }
    QUuid connectionId() const { return m_connection.id; }
    Connection connection() const { return m_connection; }
    void setConnection(const Connection &connection);
    SessionCredentials credentials() const { return m_credentials; }
    void setCredentials(const SessionCredentials &credentials);
    SessionState state() const { return m_state; }
    QDateTime connectedAt() const { return m_connectedAt; }
    QString lastError() const { return m_lastError; }
    QString displayName() const;
    bool isSftpAvailable() const { return m_file.available; }
    bool isRemoteFsAvailable() const { return m_file.available; }
    FsBackend fileBackend() const { return m_file.backend; }
    QString sftpUnavailableReason() const { return m_file.unavailableReason; }

    QList<ShellChannelState> shells() const;
    QUuid activeShellId() const { return m_activeShellId; }
    FileChannelState file() const { return m_file; }
    QList<TunnelChannelState> tunnels() const;

    void connectTransport(int cols = 80, int rows = 24);
    void disconnectTransport();
    void reconnect(int cols = 80, int rows = 24);
    void shutdown();

    QUuid newShell(int cols = 80, int rows = 24);
    void closeShell(const QUuid &shellId);
    void setActiveShell(const QUuid &shellId);
    void renameShell(const QUuid &shellId, const QString &title);
    void writeToActiveShell(const QByteArray &data);
    void writeToShell(const QUuid &shellId, const QByteArray &data);
    void changePtySize(const QUuid &shellId, int cols, int rows);

    void listDirectory(const QString &path);
    void createDirectory(const QString &path);
    void renamePath(const QString &from, const QString &to);
    void removePath(const QString &path, bool recursive);
    void uploadFiles(const QStringList &localPaths, const QString &remoteDir);
    void uploadFileTo(const QString &localPath, const QString &remotePath);
    void downloadPaths(const QStringList &remotePaths, const QString &localDir);
    void canonicalizePath(const QString &path);
    void cancelTransfer();
    void resumeInterruptedTransfer();
    void discardInterruptedTransfer();
    bool hasResumableTransfer() const { return m_hasResumableTransfer; }

    void startTunnel(const TunnelDefinition &def);
    void stopTunnel(const QUuid &tunnelId);
    void stopAllTunnels();
    void startEnabledTunnels();

    void respondHostKeyTrust(bool accept);
    SshWorker *worker() const { return m_worker; }

signals:
    void stateChanged(SessionState state);
    void shellsChanged();
    void activeShellChanged(const QUuid &shellId);
    void fileChanged();
    void tunnelsChanged();
    void shellData(const QUuid &shellId, const QByteArray &data);
    void hostKeyPrompt(SshWorker::HostKeyPrompt reason,
                       const QString &fingerprintSha256,
                       const QString &contextLabel);
    void statusMessage(const QString &message, int level = 0);
    void sessionFailed(const QString &message);

    void directoryListed(const QString &path, const QVector<RemoteEntry> &entries);
    void pathCanonicalized(const QString &requested, const QString &canonical);
    void sftpFinished(const QString &message);
    void sftpError(const QString &message);
    void sftpCanceled(const QString &message);
    void sftpInterrupted(const TransferJob &job);
    void sftpUnavailable(const QString &message);
    void sftpProgress(qint64 bytesDone, qint64 bytesTotal, const QString &currentName);
    void transferResumableChanged(bool resumable);

    void tunnelStatusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void tunnelError(const QUuid &tunnelId, const QString &message);

private:
    void setState(SessionState state);
    void ensureWorker();
    void teardownWorker();
    void wireWorker();
    int nextShellSerial();
    ShellChannelState *findShell(const QUuid &shellId);
    void updateTunnelStatus(const QUuid &tunnelId, TunnelRunStatus status, const QString &detail);
    void onWorkerConnected(const QUuid &initialShellId);
    void onWorkerDisconnected();
    void onWorkerError(const QString &message);
    void scheduleAutoReconnect();
    void tryAutoResumeTransfer();
    void onShellOpened(const QUuid &shellId);
    void onShellClosed(const QUuid &shellId);
    void onShellFailed(const QUuid &shellId, const QString &message);
    void onShellOpenFailed(const QUuid &shellId, const QString &message);

    Connection m_connection;
    SessionCredentials m_credentials;
    SessionState m_state = SessionState::Disconnected;
    QDateTime m_connectedAt;
    QString m_lastError;

    QList<ShellChannelState> m_shells;
    QUuid m_activeShellId;
    int m_nextShellSerial = 1;
    FileChannelState m_file;
    QHash<QUuid, TunnelChannelState> m_tunnels;

    QThread *m_thread = nullptr;
    SshWorker *m_worker = nullptr;
    bool m_shuttingDown = false;
    bool m_userDisconnect = false;
    bool m_autoReconnectAttempted = false;
    bool m_autoResumeAttempted = false;
    bool m_hasResumableTransfer = false;
    int m_reconnectCols = 80;
    int m_reconnectRows = 24;
};
