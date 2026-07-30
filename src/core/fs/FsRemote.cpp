#include "FsRemote.h"

#include "SftpEngine.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>

#include <cerrno>
#include <sys/stat.h>

#ifndef S_IRWXU
#define S_IRUSR 00400u
#define S_IWUSR 00200u
#define S_IXUSR 00100u
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif

namespace
{
constexpr qint64 kProgressEmitBytes = 64 * 1024;
constexpr qint64 kProgressEmitMs = 100;

QString trFs(const char *text)
{
    return QCoreApplication::translate("FsRemote", text);
}
} // namespace

FsRemote::~FsRemote()
{
    close();
}

void FsRemote::setEngine(std::unique_ptr<FsEngine> engine)
{
    close();
    m_engine = std::move(engine);
}

bool FsRemote::open(ssh_session session, QString *failureMessage)
{
    if (!m_engine) {
        m_engine = std::make_unique<SftpEngine>();
    }
    return m_engine->open(session, failureMessage);
}

void FsRemote::close()
{
    if (m_engine) {
        m_engine->close();
    }
    endTransfer();
}

bool FsRemote::isOpen() const
{
    return m_engine && m_engine->isOpen();
}

void FsRemote::setProgressCallback(ProgressCallback callback)
{
    m_progressCallback = std::move(callback);
}

void FsRemote::requestCancel()
{
    m_transferCancel.store(true, std::memory_order_relaxed);
}

bool FsRemote::wasCanceled() const
{
    return m_transferCancel.load(std::memory_order_relaxed);
}

void FsRemote::beginTransfer(qint64 bytesTotal)
{
    m_transferCancel.store(false, std::memory_order_relaxed);
    m_progressBytesDone = 0;
    m_progressBytesTotal = bytesTotal;
    m_progressLastEmitBytes = 0;
    m_progressLastEmitMs = QDateTime::currentMSecsSinceEpoch();
    if (m_progressCallback) {
        m_progressCallback(0, m_progressBytesTotal, QString());
    }
}

void FsRemote::endTransfer()
{
    m_transferCancel.store(false, std::memory_order_relaxed);
}

bool FsRemote::transferCanceled(QString *error) const
{
    if (!m_transferCancel.load(std::memory_order_relaxed)) {
        return false;
    }
    if (error) {
        *error = trFs("Transfer canceled");
    }
    return true;
}

void FsRemote::noteTransferProgress(qint64 bytesDelta, const QString &currentName)
{
    if (bytesDelta > 0) {
        m_progressBytesDone += bytesDelta;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool bytesThreshold =
        (m_progressBytesDone - m_progressLastEmitBytes) >= kProgressEmitBytes;
    const bool timeThreshold = (now - m_progressLastEmitMs) >= kProgressEmitMs;
    if (!bytesThreshold && !timeThreshold && bytesDelta > 0) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        return;
    }

    m_progressLastEmitBytes = m_progressBytesDone;
    m_progressLastEmitMs = now;
    if (m_progressCallback) {
        m_progressCallback(m_progressBytesDone, m_progressBytesTotal, currentName);
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

QString FsRemote::joinRemotePath(const QString &dir, const QString &name)
{
    if (dir.isEmpty() || dir == QLatin1String(".")) {
        return name;
    }
    if (dir.endsWith(QLatin1Char('/'))) {
        return dir + name;
    }
    return dir + QLatin1Char('/') + name;
}

bool FsRemote::listDirectoryEntries(const QString &path,
                                    QVector<RemoteEntry> *outEntries,
                                    QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->listDirectoryEntries(path, outEntries, error);
}

bool FsRemote::createDirectory(const QString &path, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->createDirectory(path, error);
}

bool FsRemote::renamePath(const QString &from, const QString &to, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->renamePath(from, to, error);
}

bool FsRemote::canonicalizePath(const QString &path, QString *canonicalOut, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }
    return m_engine->canonicalizePath(path, canonicalOut, error);
}

bool FsRemote::removePath(const QString &path, bool recursive, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    bool isDir = false;
    if (!m_engine->isRemoteDirectory(path, &isDir, error)) {
        return false;
    }

    if (isDir) {
        if (recursive) {
            return removePathRecursive(path, error);
        }
        return m_engine->removeDirectory(path, error);
    }
    return m_engine->removeFile(path, error);
}

qint64 FsRemote::computeLocalBytes(const QStringList &localPaths) const
{
    qint64 total = 0;
    for (const QString &path : localPaths) {
        const qint64 part = computeLocalPathBytes(path);
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

qint64 FsRemote::computeLocalPathBytes(const QString &localPath) const
{
    const QFileInfo info(localPath);
    if (!info.exists()) {
        return 0;
    }
    if (!info.isDir()) {
        return info.size();
    }

    qint64 total = 0;
    const QDir dir(localPath);
    const auto children = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &child : children) {
        const qint64 part = computeLocalPathBytes(child.absoluteFilePath());
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

qint64 FsRemote::computeRemoteBytes(const QStringList &remotePaths)
{
    qint64 total = 0;
    for (const QString &path : remotePaths) {
        bool isDir = false;
        QString error;
        if (!m_engine->isRemoteDirectory(path, &isDir, &error)) {
            return -1;
        }
        const qint64 part = computeRemotePathBytes(path, isDir);
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

qint64 FsRemote::computeRemotePathBytes(const QString &remotePath, bool isDir)
{
    if (!isDir) {
        qint64 size = 0;
        QString error;
        if (!m_engine->remoteFileSize(remotePath, &size, &error)) {
            return -1;
        }
        return size;
    }

    QVector<RemoteEntry> children;
    QString error;
    if (!m_engine->listDirectoryEntries(remotePath, &children, &error)) {
        return -1;
    }

    qint64 total = 0;
    for (const RemoteEntry &child : children) {
        const qint64 part = computeRemotePathBytes(child.path, child.isDir);
        if (part < 0) {
            return -1;
        }
        total += part;
    }
    return total;
}

bool FsRemote::uploadFiles(const QStringList &localPaths, const QString &remoteDir, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    beginTransfer(computeLocalBytes(localPaths));

    for (const QString &localPath : localPaths) {
        if (transferCanceled(error)) {
            endTransfer();
            return false;
        }

        const QFileInfo info(localPath);
        if (!info.exists()) {
            endTransfer();
            if (error) {
                *error = trFs("Local path does not exist: %1").arg(localPath);
            }
            return false;
        }

        const QString remotePath = joinRemotePath(remoteDir, info.fileName());
        if (!uploadPathRecursive(localPath, remotePath, error)) {
            endTransfer();
            return false;
        }
    }

    endTransfer();
    return true;
}

bool FsRemote::uploadFileTo(const QString &localPath, const QString &remotePath, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    const QFileInfo info(localPath);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = trFs("Local file does not exist: %1").arg(localPath);
        }
        return false;
    }

    beginTransfer(info.size());

    const auto shouldCancel = [this](QString *err) { return transferCanceled(err); };
    const auto onProgress = [this](qint64 delta, const QString &name) {
        noteTransferProgress(delta, name);
    };

    if (!m_engine->uploadFile(localPath, remotePath, shouldCancel, onProgress, error)) {
        endTransfer();
        return false;
    }

    endTransfer();
    return true;
}

bool FsRemote::downloadPaths(const QStringList &remotePaths, const QString &localDir, QString *error)
{
    if (!isOpen()) {
        if (error) {
            *error = trFs("Remote FS is not available");
        }
        return false;
    }

    QDir local(localDir);
    if (!local.exists() && !local.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = trFs("Cannot create local directory: %1").arg(localDir);
        }
        return false;
    }

    beginTransfer(computeRemoteBytes(remotePaths));

    for (const QString &remotePath : remotePaths) {
        if (transferCanceled(error)) {
            endTransfer();
            return false;
        }

        bool isDir = false;
        if (!m_engine->isRemoteDirectory(remotePath, &isDir, error)) {
            endTransfer();
            return false;
        }

        const QString name = QFileInfo(remotePath).fileName();
        const QString localPath = local.filePath(name);
        if (!downloadPathRecursive(remotePath, localPath, isDir, error)) {
            endTransfer();
            return false;
        }
    }

    endTransfer();
    return true;
}

bool FsRemote::removePathRecursive(const QString &path, QString *error)
{
    bool isDir = false;
    if (!m_engine->isRemoteDirectory(path, &isDir, error)) {
        return false;
    }

    if (isDir) {
        QVector<RemoteEntry> children;
        if (!m_engine->listDirectoryEntries(path, &children, error)) {
            return false;
        }
        for (const RemoteEntry &child : children) {
            if (!removePathRecursive(child.path, error)) {
                return false;
            }
        }
        return m_engine->removeDirectory(path, error);
    }

    return m_engine->removeFile(path, error);
}

bool FsRemote::uploadPathRecursive(const QString &localPath,
                                   const QString &remotePath,
                                   QString *error)
{
    if (transferCanceled(error)) {
        return false;
    }

    const QFileInfo info(localPath);
    if (info.isDir()) {
        QString createError;
        if (!m_engine->createDirectory(remotePath, &createError)) {
            bool existsAsDir = false;
            QString statError;
            if (!(m_engine->isRemoteDirectory(remotePath, &existsAsDir, &statError) && existsAsDir)) {
                if (error) {
                    *error = createError.isEmpty()
                                 ? trFs("Cannot create remote folder: %1").arg(statError)
                                 : createError;
                }
                return false;
            }
        }

        const QDir dir(localPath);
        const auto children = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &child : children) {
            const QString childRemote = joinRemotePath(remotePath, child.fileName());
            if (!uploadPathRecursive(child.absoluteFilePath(), childRemote, error)) {
                return false;
            }
        }
        return true;
    }

    const auto shouldCancel = [this](QString *err) { return transferCanceled(err); };
    const auto onProgress = [this](qint64 delta, const QString &name) {
        noteTransferProgress(delta, name);
    };
    return m_engine->uploadFile(localPath, remotePath, shouldCancel, onProgress, error);
}

bool FsRemote::downloadPathRecursive(const QString &remotePath,
                                     const QString &localPath,
                                     bool isDir,
                                     QString *error)
{
    if (transferCanceled(error)) {
        return false;
    }

    if (isDir) {
        QDir local(localPath);
        if (!local.exists() && !QDir().mkpath(localPath)) {
            if (error) {
                *error = (errno == ENOSPC) ? trFs("Disk full")
                                           : trFs("Cannot create local folder: %1").arg(localPath);
            }
            return false;
        }

        QVector<RemoteEntry> children;
        if (!m_engine->listDirectoryEntries(remotePath, &children, error)) {
            return false;
        }

        for (const RemoteEntry &child : children) {
            const QString childLocal = QDir(localPath).filePath(child.name);
            if (!downloadPathRecursive(child.path, childLocal, child.isDir, error)) {
                return false;
            }
        }
        return true;
    }

    const auto shouldCancel = [this](QString *err) { return transferCanceled(err); };
    const auto onProgress = [this](qint64 delta, const QString &name) {
        noteTransferProgress(delta, name);
    };
    return m_engine->downloadFile(remotePath, localPath, shouldCancel, onProgress, error);
}
