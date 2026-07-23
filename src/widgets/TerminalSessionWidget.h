#pragma once

#include "Connection.h"
#include "SftpTypes.h"
#include "SshWorker.h"
#include "Tunnel.h"

#include <QByteArray>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <QWidget>

class QEvent;
class QLabel;
class QSocketNotifier;
class QTermWidget;
class QThread;
class QTimer;

class TerminalSessionWidget final : public QWidget {
    Q_OBJECT

public:
    enum class State {
        Connecting,
        Connected,
        Disconnected,
        Failed,
    };
    Q_ENUM(State)

    explicit TerminalSessionWidget(QWidget *parent = nullptr);
    ~TerminalSessionWidget() override;

    void start(const Connection &connection, const QString &secret);
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
    bool isSftpAvailable() const;
    QString sftpUnavailableReason() const;

signals:
    void statusMessage(const QString &message);
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
    void onHostKeyPrompt(SshWorker::HostKeyPrompt reason, const QString &fingerprint);
    void onErrorOccurred(const QString &message);
    void onDisconnected();
    void onSendData(const char *data, int length);
    void syncPtySize();
    void flushPendingDisplay();

private:
    void setState(State state);
    void beginConnect();
    void shutdownWorker();
    void showConnectingState();
    void showDisconnectedState();
    void showErrorState(const QString &message);
    void setupPtyBridge();
    void teardownPtyBridge();
    void clearSecret();
    void readTerminalSize(int *cols, int *rows) const;
    void syncLocalPtyWinsize(int cols, int rows);
    void schedulePtySizeSync();

    Connection m_connection;
    QString m_secret;
    QString m_displayName;
    State m_state = State::Disconnected;
    QTermWidget *m_term = nullptr;
    QLabel *m_overlay = nullptr;
    QThread *m_thread = nullptr;
    SshWorker *m_worker = nullptr;
    QTimer *m_resizeDebounce = nullptr;
    QTimer *m_flushTimer = nullptr;
    QSocketNotifier *m_ptyWriteNotifier = nullptr;
    QByteArray m_pendingDisplay;
    bool m_teletypeStarted = false;
    bool m_shuttingDown = false;
    bool m_sftpAvailable = false;
    QString m_sftpUnavailableReason;
    int m_lastCols = 0;
    int m_lastRows = 0;
};
