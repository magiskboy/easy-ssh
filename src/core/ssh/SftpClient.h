#pragma once

#include "SftpTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

/**
 * Owns an sftp_session and implements remote filesystem / transfer operations.
 * Not a QObject — progress and errors are reported via callbacks / out-params;
 * SshWorker remains the sole signal emitter for the GUI.
 */
class SftpClient
{
public:
    using ProgressCallback =
        std::function<void(qint64 bytesDone, qint64 bytesTotal, const QString &currentName)>;

    SftpClient() = default;
    ~SftpClient();

    SftpClient(const SftpClient &) = delete;
    SftpClient &operator=(const SftpClient &) = delete;

    bool open(ssh_session session, QString *failureMessage = nullptr);
    void close();
    bool isOpen() const { return m_sftp != nullptr; }

    void requestCancel();
    void setProgressCallback(ProgressCallback callback);

    bool listDirectoryEntries(const QString &path,
                              QVector<RemoteEntry> *outEntries,
                              QString *error);
    bool createDirectory(const QString &path, QString *error);
    bool renamePath(const QString &from, const QString &to, QString *error);
    bool removePath(const QString &path, bool recursive, QString *error);
    bool canonicalizePath(const QString &path, QString *canonicalOut, QString *error);

    /// Upload local paths into remoteDir. On cancel/error fills *error; returns false.
    bool uploadFiles(const QStringList &localPaths, const QString &remoteDir, QString *error);
    bool uploadFileTo(const QString &localPath, const QString &remotePath, QString *error);
    bool downloadPaths(const QStringList &remotePaths, const QString &localDir, QString *error);

    bool wasCanceled() const;

private:
    bool openSftpSession(ssh_session session, QString *failureMessage);
    QString sessionErrorOf(ssh_session session) const;
    QString sftpErrorMessage() const;
    static QString localIoErrorMessage(const QString &qtErrorString);
    static QString formatPermissions(uint32_t permissions, uint8_t type);

    void beginTransfer(qint64 bytesTotal);
    void endTransfer();
    bool transferCanceled(QString *error) const;
    void noteTransferProgress(qint64 bytesDelta, const QString &currentName);

    qint64 computeLocalBytes(const QStringList &localPaths) const;
    qint64 computeLocalPathBytes(const QString &localPath) const;
    qint64 computeRemoteBytes(const QStringList &remotePaths);
    qint64 computeRemotePathBytes(const QString &remotePath, bool isDir);

    bool removePathRecursive(const QString &path, QString *error);
    bool uploadPathRecursive(const QString &localPath, const QString &remotePath, QString *error);
    bool downloadPathRecursive(const QString &remotePath,
                               const QString &localPath,
                               bool isDir,
                               QString *error);
    bool uploadFile(const QString &localPath, const QString &remotePath, QString *error);
    bool downloadFile(const QString &remotePath, const QString &localPath, QString *error);
    bool isRemoteDirectory(const QString &path, bool *isDir, QString *error);

    sftp_session m_sftp = nullptr;
    ssh_session m_session = nullptr;

    ProgressCallback m_progressCallback;
    std::atomic_bool m_transferCancel{false};
    qint64 m_progressBytesDone = 0;
    qint64 m_progressBytesTotal = -1;
    qint64 m_progressLastEmitBytes = 0;
    qint64 m_progressLastEmitMs = 0;
};
