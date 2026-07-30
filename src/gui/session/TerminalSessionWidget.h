/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"
#include "core/fs/SftpTypes.h"
#include "core/ssh/SshWorker.h"
#include "core/tunnel/Tunnel.h"
#include "gui/ErrorNotifier.h"

#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <QWidget>

class QEvent;
class QLabel;
class QPushButton;
class QTermWidget;
class QThread;
class QTimer;
class TerminalIoBridge;

class TerminalSessionWidget final : public QWidget
{
    Q_OBJECT

public:
    enum class State
    {
        Connecting,
        Connected,
        Disconnected,
        Failed,
    };
    Q_ENUM(State)

    explicit TerminalSessionWidget(QWidget *parent = nullptr);
    ~TerminalSessionWidget() override;

    void start(const Connection &connection, const SessionCredentials &credentials);
    void disconnectSession();
    void reconnect();
    void applySettings();

    void copySelection();
    void pasteClipboard();
    void toggleSearch();
    void clearScreen();
    void saveLog();
    void saveScreenshot();

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
    void startEnabledTunnels();

    QString displayName() const;
    QUuid connectionId() const;
    Connection connection() const;
    State sessionState() const;
    QDateTime connectedAt() const;
    bool isSftpAvailable() const;
    QString sftpUnavailableReason() const;

signals:
    void statusMessage(const QString &message, ErrorNotifier::Level level);
    void sessionFailed(const QString &message);
    void sessionDisconnected();
    void sessionStateChanged(TerminalSessionWidget::State state);

    void directoryListed(const QString &path, const QVector<RemoteEntry> &entries);
    void pathCanonicalized(const QString &requested, const QString &canonical);
    void sftpFinished(const QString &message);
    void sftpError(const QString &message);
    void sftpCanceled(const QString &message);
    void sftpUnavailable(const QString &message);
    void sftpProgress(qint64 bytesDone, qint64 bytesTotal, const QString &currentName);

    void tunnelStatusChanged(const QUuid &tunnelId, const QString &status, const QString &detail);
    void tunnelError(const QUuid &tunnelId, const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onConnected();
    void onDataReceived(const QByteArray &data);
    void onHostKeyPrompt(SshWorker::HostKeyPrompt reason,
                         const QString &fingerprint,
                         const QString &contextLabel);
    void onErrorOccurred(const QString &message);
    void onDisconnected();
    void onSendData(const char *data, int length);
    void syncPtySize();

private:
    void setState(State state);
    void beginConnect();
    void shutdownWorker();
    void showConnectingState();
    void showDisconnectedState();
    void showErrorState(const QString &message);
    void showOverlay(const QString &message, bool showReconnect);
    void clearSecret();
    void readTerminalSize(int *cols, int *rows) const;
    void schedulePtySizeSync();

    Connection m_connection;
    SessionCredentials m_credentials;
    QString m_displayName;
    State m_state = State::Disconnected;
    QDateTime m_connectedAt;
    QTermWidget *m_term = nullptr;
    QWidget *m_overlayPanel = nullptr;
    QLabel *m_overlayLabel = nullptr;
    QPushButton *m_reconnectButton = nullptr;
    QThread *m_thread = nullptr;
    SshWorker *m_worker = nullptr;
    QTimer *m_resizeDebounce = nullptr;
    TerminalIoBridge *m_ioBridge = nullptr;
    bool m_teletypeStarted = false;
    bool m_shuttingDown = false;
    bool m_sftpAvailable = false;
    QString m_sftpUnavailableReason;
    int m_lastCols = 0;
    int m_lastRows = 0;
};
