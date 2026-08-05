// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SftpEngine.h"

#include "Symlink.h"
#include "TransferTypes.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef S_IRUSR
#define S_IRUSR 00400u
#define S_IWUSR 00200u
#define S_IXUSR 00100u
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#define S_IRGRP 00040u
#define S_IWGRP 00020u
#define S_IXGRP 00010u
#define S_IROTH 00004u
#define S_IWOTH 00002u
#define S_IXOTH 00001u
#endif

namespace
{
constexpr size_t kXferBufSize = 16384;

QString trSftp(const char *text)
{
    return QCoreApplication::translate("SftpEngine", text);
}
} // namespace

SftpEngine::~SftpEngine()
{
    close();
}

FsEngine::Capabilities SftpEngine::capabilities() const
{
    return List | Mkdir | Rename | Remove | Canonicalize | Transfer | ResumeTransfer | Symlink;
}

bool SftpEngine::open(ssh_session session, QString *failureMessage)
{
    close();
    m_session = session;

    m_sftp = sftp_new(session);
    if (m_sftp == nullptr) {
        if (failureMessage) {
            *failureMessage =
                trSftp("Failed to create SFTP session: %1").arg(sessionErrorOf(session));
        }
        return false;
    }

    if (sftp_init(m_sftp) != SSH_OK) {
        if (failureMessage) {
            const QString detail =
                sftpErrorMessage().isEmpty() ? sessionErrorOf(session) : sftpErrorMessage();
            if (detail.contains(QStringLiteral("subsystem"), Qt::CaseInsensitive) ||
                sessionErrorOf(session).contains(QStringLiteral("subsystem"),
                                                 Qt::CaseInsensitive)) {
                *failureMessage =
                    trSftp("This server does not support SFTP (subsystem request failed).");
            } else {
                *failureMessage = trSftp("SFTP is unavailable: %1").arg(detail);
            }
        }
        sftp_free(m_sftp);
        m_sftp = nullptr;
        return false;
    }

    return true;
}

void SftpEngine::close()
{
    if (m_sftp) {
        sftp_free(m_sftp);
        m_sftp = nullptr;
    }
    m_session = nullptr;
}

QString SftpEngine::sessionErrorOf(ssh_session session) const
{
    if (session == nullptr) {
        return trSftp("Unknown error");
    }
    const char *err = ssh_get_error(session);
    return err ? QString::fromUtf8(err) : trSftp("Unknown error");
}

QString SftpEngine::sftpErrorMessage() const
{
    if (m_sftp == nullptr) {
        return sessionErrorOf(m_session);
    }

    const int code = sftp_get_error(m_sftp);
    switch (code) {
    case SSH_FX_OK:
        return sessionErrorOf(m_session);
    case SSH_FX_NO_SUCH_FILE:
        return trSftp("No such file or directory");
    case SSH_FX_PERMISSION_DENIED:
        return trSftp("Permission denied");
    case SSH_FX_FAILURE:
        return trSftp("SFTP failure");
    case SSH_FX_BAD_MESSAGE:
        return trSftp("Bad SFTP message");
    case SSH_FX_NO_CONNECTION:
        return trSftp("No SFTP connection");
    case SSH_FX_CONNECTION_LOST:
        return trSftp("SFTP connection lost");
    case SSH_FX_OP_UNSUPPORTED:
        return trSftp("SFTP operation unsupported");
    case SSH_FX_INVALID_HANDLE:
        return trSftp("Invalid SFTP handle");
    case SSH_FX_NO_SUCH_PATH:
        return trSftp("No such path");
    case SSH_FX_FILE_ALREADY_EXISTS:
        return trSftp("File already exists");
    case SSH_FX_WRITE_PROTECT:
        return trSftp("Write-protected filesystem");
#ifdef SSH_FX_NO_SPACE_ON_FILESYSTEM
    case SSH_FX_NO_SPACE_ON_FILESYSTEM:
        return trSftp("Disk full");
#endif
    default:
        return sessionErrorOf(m_session).isEmpty() ? trSftp("SFTP error %1").arg(code)
                                                   : sessionErrorOf(m_session);
    }
}

QString SftpEngine::localIoErrorMessage(const QString &qtErrorString)
{
    if (errno == ENOSPC ||
        qtErrorString.contains(QStringLiteral("No space"), Qt::CaseInsensitive) ||
        qtErrorString.contains(QStringLiteral("disk full"), Qt::CaseInsensitive)) {
        return trSftp("Disk full");
    }
    return qtErrorString;
}

QString SftpEngine::formatPermissions(uint32_t permissions, EntryType type)
{
    QString result(10, QLatin1Char('-'));

    switch (type) {
    case EntryType::Directory:
        result[0] = QLatin1Char('d');
        break;
    case EntryType::Symlink:
        result[0] = QLatin1Char('l');
        break;
    case EntryType::Special:
        result[0] = QLatin1Char('s');
        break;
    case EntryType::Regular:
        break;
    }

    const auto setBit = [&](int index, QChar ch, uint32_t mask) {
        if (permissions & mask) {
            result[index] = ch;
        }
    };

    setBit(1, QLatin1Char('r'), S_IRUSR);
    setBit(2, QLatin1Char('w'), S_IWUSR);
    setBit(3, QLatin1Char('x'), S_IXUSR);
    setBit(4, QLatin1Char('r'), S_IRGRP);
    setBit(5, QLatin1Char('w'), S_IWGRP);
    setBit(6, QLatin1Char('x'), S_IXGRP);
    setBit(7, QLatin1Char('r'), S_IROTH);
    setBit(8, QLatin1Char('w'), S_IWOTH);
    setBit(9, QLatin1Char('x'), S_IXOTH);
    return result;
}

QString SftpEngine::joinRemotePath(const QString &dir, const QString &name)
{
    if (dir.isEmpty() || dir == QLatin1String(".")) {
        return name;
    }
    if (dir.endsWith(QLatin1Char('/'))) {
        return dir + name;
    }
    return dir + QLatin1Char('/') + name;
}

void SftpEngine::fillEntryFromAttributes(RemoteEntry *entry,
                                         const sftp_attributes attributes,
                                         const QString &path,
                                         const QString &name)
{
    entry->name = name;
    entry->path = path;
    const auto type = static_cast<EntryType>(attributes->type);
    entry->isSymlink = type == EntryType::Symlink;
    entry->isDir = !entry->isSymlink && type == EntryType::Directory;
    entry->size = static_cast<qint64>(attributes->size);
    entry->permissions = formatPermissions(attributes->permissions, type);
    if (attributes->flags & SSH_FILEXFER_ATTR_ACMODTIME) {
        entry->mtime = static_cast<qint64>(attributes->mtime);
    } else if (attributes->mtime64 != 0) {
        entry->mtime = static_cast<qint64>(attributes->mtime64);
    } else {
        entry->mtime = 0;
    }
}

QString SftpEngine::readlinkAt(const QString &path) const
{
    const QByteArray remote = path.toUtf8();
    char *target = sftp_readlink(m_sftp, remote.constData());
    if (target == nullptr) {
        return {};
    }
    const QString out = QString::fromUtf8(target);
    ssh_string_free_char(target);
    return out;
}

QString SftpEngine::directoryOpenPath(const QString &path)
{
    // Some SFTP servers (e.g. Synology) do not follow symlinks in OPENDIR.
    // If this path is a symlink to a directory, open the canonical target instead
    // while still attributing child paths under the caller's path.
    RemoteEntry meta;
    QString unused;
    if (!statEntry(path, &meta, false, &unused) || !meta.isSymlink) {
        return path;
    }
    RemoteEntry followed;
    if (!statEntry(path, &followed, true, &unused) || !followed.isDir) {
        return path;
    }
    QString canonical;
    if (canonicalizePath(path, canonical, &unused) && !canonical.isEmpty()) {
        return canonical;
    }
    if (!path.endsWith(QLatin1Char('/'))) {
        return path + QLatin1Char('/');
    }
    return path;
}

bool SftpEngine::listDirectoryEntries(const QString &path,
                                      QVector<RemoteEntry> *outEntries,
                                      QString *error)
{
    const QString openPath = directoryOpenPath(path);
    const QByteArray remote = openPath.toUtf8();
    sftp_dir dir = sftp_opendir(m_sftp, remote.constData());
    if (dir == nullptr) {
        if (error) {
            *error = trSftp("Cannot open directory: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    QVector<RemoteEntry> entries;
    while (sftp_attributes attributes = sftp_readdir(m_sftp, dir)) {
        const QString name = QString::fromUtf8(attributes->name);
        if (name == QLatin1String(".") || name == QLatin1String("..")) {
            sftp_attributes_free(attributes);
            continue;
        }

        RemoteEntry entry;
        fillEntryFromAttributes(&entry, attributes, joinRemotePath(path, name), name);
        if (entry.isSymlink) {
            entry.linkTarget = readlinkAt(entry.path);
            const QByteArray entryRemote = entry.path.toUtf8();
            if (sftp_attributes followed = sftp_stat(m_sftp, entryRemote.constData())) {
                entry.linkIsDir = followed->type == SSH_FILEXFER_TYPE_DIRECTORY;
                sftp_attributes_free(followed);
            }
        }
        entries.append(entry);
        sftp_attributes_free(attributes);
    }

    if (!sftp_dir_eof(dir)) {
        if (error) {
            *error = trSftp("Cannot list directory: %1").arg(sftpErrorMessage());
        }
        sftp_closedir(dir);
        return false;
    }

    if (sftp_closedir(dir) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot close directory: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    std::sort(entries.begin(), entries.end(), [](const RemoteEntry &a, const RemoteEntry &b) {
        const bool aDir = Symlink::isDirectoryLike(a);
        const bool bDir = Symlink::isDirectoryLike(b);
        if (aDir != bDir) {
            return aDir;
        }
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    if (outEntries) {
        *outEntries = entries;
    }
    return true;
}

bool SftpEngine::createDirectory(const QString &path, QString *error)
{
    const QByteArray remote = path.toUtf8();
    if (sftp_mkdir(m_sftp, remote.constData(), S_IRWXU) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot create folder: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SftpEngine::renamePath(const QString &from, const QString &to, QString *error)
{
    const QByteArray src = from.toUtf8();
    const QByteArray dst = to.toUtf8();
    if (sftp_rename(m_sftp, src.constData(), dst.constData()) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot rename: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SftpEngine::removeFile(const QString &path, QString *error)
{
    const QByteArray remote = path.toUtf8();
    if (sftp_unlink(m_sftp, remote.constData()) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot delete file: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SftpEngine::removeDirectory(const QString &path, QString *error)
{
    const QByteArray remote = path.toUtf8();
    if (sftp_rmdir(m_sftp, remote.constData()) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot delete folder: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SftpEngine::canonicalizePath(const QString &path, QString &canonicalOut, QString *error)
{
    Q_UNUSED(error);
    const QString requested = path.isEmpty() ? QStringLiteral(".") : path;
    const QByteArray remote = requested.toUtf8();
    char *canonical = sftp_canonicalize_path(m_sftp, remote.constData());
    if (canonical == nullptr) {
        canonicalOut = requested;
        return true;
    }

    canonicalOut = QString::fromUtf8(canonical);
    ssh_string_free_char(canonical);
    return true;
}

bool SftpEngine::statEntry(const QString &path, RemoteEntry *out, bool follow, QString *error)
{
    if (!out) {
        if (error) {
            *error = trSftp("Internal error: missing entry");
        }
        return false;
    }

    const QByteArray remote = path.toUtf8();
    sftp_attributes attributes =
        follow ? sftp_stat(m_sftp, remote.constData()) : sftp_lstat(m_sftp, remote.constData());
    if (attributes == nullptr) {
        if (error) {
            *error = trSftp("Cannot stat path: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    const QString name = QFileInfo(path).fileName();
    fillEntryFromAttributes(out, attributes, path, name.isEmpty() ? path : name);
    if (out->isSymlink) {
        out->linkTarget = readlinkAt(path);
    } else {
        out->linkTarget.clear();
    }
    sftp_attributes_free(attributes);
    return true;
}

bool SftpEngine::isRemoteDirectory(const QString &path, bool *isDir, QString *error)
{
    RemoteEntry entry;
    if (!statEntry(path, &entry, true, error)) {
        return false;
    }
    if (isDir) {
        *isDir = entry.isDir;
    }
    return true;
}

bool SftpEngine::remoteFileSize(const QString &path, qint64 *sizeOut, QString *error)
{
    RemoteEntry entry;
    if (!statEntry(path, &entry, true, error)) {
        return false;
    }
    if (sizeOut) {
        *sizeOut = entry.size;
    }
    return true;
}

bool SftpEngine::createSymlink(const QString &target, const QString &linkPath, QString *error)
{
    const QByteArray targetBytes = target.toUtf8();
    const QByteArray linkBytes = linkPath.toUtf8();
    if (sftp_symlink(m_sftp, targetBytes.constData(), linkBytes.constData()) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot create symlink: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SftpEngine::readSymlink(const QString &path, QString &targetOut, QString *error)
{
    const QString target = readlinkAt(path);
    if (target.isEmpty() && sftp_get_error(m_sftp) != SSH_FX_OK) {
        if (error) {
            *error = trSftp("Cannot read symlink: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    targetOut = target;
    return true;
}

bool SftpEngine::feedHashFromLocal(QFile &local,
                                   qint64 length,
                                   QCryptographicHash *hash,
                                   QString *error) const
{
    if (!hash || length < 0) {
        if (error) {
            *error = trSftp("Invalid hash request");
        }
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (!local.seek(0)) {
        if (error) {
            *error =
                trSftp("Cannot seek local file: %1").arg(localIoErrorMessage(local.errorString()));
        }
        return false;
    }

    qint64 remaining = length;
    char buffer[kXferBufSize];
    while (remaining > 0) {
        const qint64 chunk = qMin(remaining, static_cast<qint64>(sizeof(buffer)));
        const qint64 nread = local.read(buffer, chunk);
        if (nread <= 0) {
            if (error) {
                *error = trSftp("Cannot read local file for hashing: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
            return false;
        }
        hash->addData(QByteArrayView(buffer, static_cast<qsizetype>(nread)));
        remaining -= nread;
    }
    return true;
}

bool SftpEngine::hashLocalPrefix(const QString &localPath,
                                 qint64 length,
                                 QString &hexOut,
                                 QString *error) const
{
    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        if (error) {
            *error =
                trSftp("Cannot open local file: %1").arg(localIoErrorMessage(local.errorString()));
        }
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!feedHashFromLocal(local, length, &hash, error)) {
        return false;
    }
    hexOut = QString::fromLatin1(hash.result().toHex());
    return true;
}

bool SftpEngine::hashLocalFile(const QString &localPath, QString &hexOut, QString *error) const
{
    const QFileInfo info(localPath);
    return hashLocalPrefix(localPath, info.size(), hexOut, error);
}

bool SftpEngine::hashRemotePrefix(const QString &remotePath,
                                  qint64 length,
                                  QString &hexOut,
                                  QString *error) const
{
    if (length < 0) {
        if (error) {
            *error = trSftp("Invalid remote hash length");
        }
        return false;
    }
    if (length == 0) {
        QCryptographicHash empty(QCryptographicHash::Sha256);
        hexOut = QString::fromLatin1(empty.result().toHex());
        return true;
    }

    const QByteArray remote = remotePath.toUtf8();
    sftp_file file = sftp_open(m_sftp, remote.constData(), O_RDONLY, 0);
    if (file == nullptr) {
        if (error) {
            *error = trSftp("Cannot open remote file for hashing: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 remaining = length;
    char buffer[kXferBufSize];
    while (remaining > 0) {
        const size_t chunk =
            static_cast<size_t>(qMin(remaining, static_cast<qint64>(sizeof(buffer))));
        const ssize_t nbytes = sftp_read(file, buffer, chunk);
        if (nbytes <= 0) {
            sftp_close(file);
            if (error) {
                *error = trSftp("Cannot read remote file for hashing: %1").arg(sftpErrorMessage());
            }
            return false;
        }
        hash.addData(QByteArrayView(buffer, static_cast<qsizetype>(nbytes)));
        remaining -= nbytes;
    }
    sftp_close(file);
    hexOut = QString::fromLatin1(hash.result().toHex());
    return true;
}

bool SftpEngine::uploadFile(const QString &localPath,
                            const CancelCheck &shouldCancel,
                            const QString &remotePath,
                            const ProgressNote &onProgress,
                            const TransferOptions &options,
                            QString *error,
                            qint64 *partialBytes,
                            QString *partialSha256PrefixHex)
{
    if (partialBytes) {
        *partialBytes = 0;
    }
    if (partialSha256PrefixHex) {
        partialSha256PrefixHex->clear();
    }

    if (shouldCancel && shouldCancel(error)) {
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        if (error) {
            *error =
                trSftp("Cannot open local file: %1").arg(localIoErrorMessage(local.errorString()));
        }
        return false;
    }

    const qint64 localSize = local.size();
    const bool useFilepart = options.mode != TransferWriteMode::OverwriteFinal;
    const QString writePath = useFilepart ? transferFilepartPathForFinal(remotePath) : remotePath;
    const QByteArray remote = writePath.toUtf8();

    qint64 offset = 0;
    QCryptographicHash running(QCryptographicHash::Sha256);

    if (options.mode == TransferWriteMode::ResumeFilepart) {
        offset = options.resumeOffset;
        if (offset < 0 || offset > localSize) {
            if (error) {
                *error = trSftp("Invalid resume offset");
            }
            return false;
        }

        qint64 remoteSize = 0;
        if (!remoteFileSize(writePath, &remoteSize, error) || remoteSize != offset) {
            if (error && error->isEmpty()) {
                *error = trSftp("Remote partial size does not match resume offset");
            }
            return false;
        }

        QString localPrefix;
        if (!hashLocalPrefix(localPath, offset, localPrefix, error)) {
            return false;
        }
        if (localPrefix.compare(options.sha256PrefixHex, Qt::CaseInsensitive) != 0) {
            if (error) {
                *error =
                    trSftp("Local file changed since the interrupted transfer (hash mismatch)");
            }
            return false;
        }

        QString remotePrefix;
        if (!hashRemotePrefix(writePath, offset, remotePrefix, error)) {
            return false;
        }
        if (remotePrefix.compare(options.sha256PrefixHex, Qt::CaseInsensitive) != 0) {
            if (error) {
                *error = trSftp("Remote partial file is corrupt (hash mismatch)");
            }
            return false;
        }

        if (!feedHashFromLocal(local, offset, &running, error)) {
            return false;
        }
        if (!local.seek(offset)) {
            if (error) {
                *error = trSftp("Cannot seek local file: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
            return false;
        }
    } else if (!local.seek(0)) {
        if (error) {
            *error =
                trSftp("Cannot seek local file: %1").arg(localIoErrorMessage(local.errorString()));
        }
        return false;
    }

    int access = O_WRONLY | O_CREAT;
    if (options.mode != TransferWriteMode::ResumeFilepart) {
        access |= O_TRUNC;
    }
    sftp_file file =
        sftp_open(m_sftp, remote.constData(), access, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file == nullptr) {
        if (error) {
            *error = trSftp("Cannot open remote file for writing: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    if (options.mode == TransferWriteMode::ResumeFilepart) {
        if (sftp_seek64(file, static_cast<uint64_t>(offset)) < 0) {
            if (error) {
                *error = trSftp("Cannot seek remote file: %1").arg(sftpErrorMessage());
            }
            sftp_close(file);
            return false;
        }
    }

    const QString displayName = QFileInfo(localPath).fileName();
    if (onProgress) {
        onProgress(0, displayName);
    }

    qint64 bytesDone = offset;
    auto persistPartial = [&]() {
        if (partialBytes) {
            *partialBytes = bytesDone;
        }
        if (partialSha256PrefixHex && bytesDone > 0) {
            QString hex;
            if (hashLocalPrefix(localPath, bytesDone, hex, nullptr)) {
                *partialSha256PrefixHex = hex;
            }
        }
    };

    char buffer[kXferBufSize];
    while (bytesDone < localSize) {
        if (shouldCancel && shouldCancel(error)) {
            persistPartial();
            sftp_close(file);
            return false;
        }

        const qint64 nread = local.read(buffer, static_cast<qint64>(sizeof(buffer)));
        if (nread < 0) {
            if (error) {
                *error = trSftp("Cannot read local file: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
            persistPartial();
            sftp_close(file);
            return false;
        }
        if (nread == 0) {
            break;
        }

        qint64 remaining = nread;
        const char *ptr = buffer;
        while (remaining > 0) {
            if (shouldCancel && shouldCancel(error)) {
                persistPartial();
                sftp_close(file);
                return false;
            }

            const ssize_t nwritten = sftp_write(file, ptr, static_cast<size_t>(remaining));
            if (nwritten < 0) {
                if (error) {
                    *error = trSftp("Cannot write remote file: %1").arg(sftpErrorMessage());
                }
                persistPartial();
                sftp_close(file);
                return false;
            }
            running.addData(QByteArrayView(ptr, static_cast<qsizetype>(nwritten)));
            ptr += nwritten;
            remaining -= nwritten;
            bytesDone += nwritten;
            if (onProgress) {
                onProgress(nwritten, displayName);
            }
        }
    }

    if (sftp_close(file) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot close remote file: %1").arg(sftpErrorMessage());
        }
        persistPartial();
        return false;
    }

    const QString streamedHex = QString::fromLatin1(running.result().toHex());
    QString expectedFull = options.expectedSha256FullHex;
    if (expectedFull.isEmpty()) {
        if (!hashLocalFile(localPath, expectedFull, error)) {
            persistPartial();
            return false;
        }
    }
    if (streamedHex.compare(expectedFull, Qt::CaseInsensitive) != 0) {
        if (error) {
            *error = trSftp("Upload hash mismatch");
        }
        persistPartial();
        return false;
    }

    if (useFilepart) {
        // Replace final path atomically via remove + rename when supported.
        QString unused;
        removeFile(remotePath, &unused);
        if (!renamePath(writePath, remotePath, error)) {
            persistPartial();
            return false;
        }
    }

    if (partialBytes) {
        *partialBytes = bytesDone;
    }
    if (partialSha256PrefixHex) {
        *partialSha256PrefixHex = streamedHex;
    }
    return true;
}

bool SftpEngine::downloadFile(const QString &remotePath,
                              const CancelCheck &shouldCancel,
                              const QString &localPath,
                              const ProgressNote &onProgress,
                              const TransferOptions &options,
                              QString *error,
                              qint64 *partialBytes,
                              QString *partialSha256PrefixHex)
{
    if (partialBytes) {
        *partialBytes = 0;
    }
    if (partialSha256PrefixHex) {
        partialSha256PrefixHex->clear();
    }

    if (shouldCancel && shouldCancel(error)) {
        return false;
    }

    qint64 remoteSize = 0;
    if (!remoteFileSize(remotePath, &remoteSize, error)) {
        return false;
    }

    const bool useFilepart = options.mode != TransferWriteMode::OverwriteFinal;
    const QString writePath = useFilepart ? transferFilepartPathForFinal(localPath) : localPath;

    const QByteArray remote = remotePath.toUtf8();
    sftp_file file = sftp_open(m_sftp, remote.constData(), O_RDONLY, 0);
    if (file == nullptr) {
        if (error) {
            *error = trSftp("Cannot open remote file for reading: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    qint64 offset = 0;
    QCryptographicHash running(QCryptographicHash::Sha256);
    QIODevice::OpenMode localMode = QIODevice::WriteOnly | QIODevice::Truncate;

    if (options.mode == TransferWriteMode::ResumeFilepart) {
        offset = options.resumeOffset;
        if (offset < 0 || offset > remoteSize) {
            sftp_close(file);
            if (error) {
                *error = trSftp("Invalid resume offset");
            }
            return false;
        }

        const QFileInfo partInfo(writePath);
        if (!partInfo.exists() || partInfo.size() != offset) {
            sftp_close(file);
            if (error) {
                *error = trSftp("Local partial size does not match resume offset");
            }
            return false;
        }

        QString localPrefix;
        if (!hashLocalPrefix(writePath, offset, localPrefix, error)) {
            sftp_close(file);
            return false;
        }
        if (localPrefix.compare(options.sha256PrefixHex, Qt::CaseInsensitive) != 0) {
            sftp_close(file);
            if (error) {
                *error = trSftp("Local partial file is corrupt (hash mismatch)");
            }
            return false;
        }

        QFile seed(writePath);
        if (!seed.open(QIODevice::ReadOnly) || !feedHashFromLocal(seed, offset, &running, error)) {
            sftp_close(file);
            return false;
        }

        if (sftp_seek64(file, static_cast<uint64_t>(offset)) < 0) {
            sftp_close(file);
            if (error) {
                *error = trSftp("Cannot seek remote file: %1").arg(sftpErrorMessage());
            }
            return false;
        }
        localMode = QIODevice::WriteOnly | QIODevice::Append;
    }

    QFile local(writePath);
    if (!local.open(localMode)) {
        if (error) {
            *error = trSftp("Cannot open local file for writing: %1")
                         .arg(localIoErrorMessage(local.errorString()));
        }
        sftp_close(file);
        return false;
    }

    const QString displayName = QFileInfo(remotePath).fileName();
    if (onProgress) {
        onProgress(0, displayName);
    }

    qint64 bytesDone = offset;
    auto persistPartial = [&]() {
        if (partialBytes) {
            *partialBytes = bytesDone;
        }
        if (partialSha256PrefixHex && bytesDone > 0) {
            local.flush();
            QString hex;
            if (hashLocalPrefix(writePath, bytesDone, hex, nullptr)) {
                *partialSha256PrefixHex = hex;
            }
        }
    };

    char buffer[kXferBufSize];
    for (;;) {
        if (shouldCancel && shouldCancel(error)) {
            persistPartial();
            sftp_close(file);
            return false;
        }

        const ssize_t nbytes = sftp_read(file, buffer, sizeof(buffer));
        if (nbytes == 0) {
            break;
        }
        if (nbytes < 0) {
            if (error) {
                *error = trSftp("Cannot read remote file: %1").arg(sftpErrorMessage());
            }
            persistPartial();
            sftp_close(file);
            return false;
        }

        if (local.write(buffer, nbytes) != nbytes) {
            if (error) {
                *error = trSftp("Cannot write local file: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
            persistPartial();
            sftp_close(file);
            return false;
        }
        running.addData(QByteArrayView(buffer, static_cast<qsizetype>(nbytes)));
        bytesDone += nbytes;
        if (onProgress) {
            onProgress(nbytes, displayName);
        }
    }

    if (sftp_close(file) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot close remote file: %1").arg(sftpErrorMessage());
        }
        persistPartial();
        return false;
    }
    local.close();

    if (bytesDone != remoteSize) {
        if (error) {
            *error = trSftp("Download size mismatch");
        }
        persistPartial();
        return false;
    }

    const QString streamedHex = QString::fromLatin1(running.result().toHex());
    QString verifyHex;
    if (!hashLocalPrefix(writePath, bytesDone, verifyHex, error) ||
        verifyHex.compare(streamedHex, Qt::CaseInsensitive) != 0) {
        if (error && error->isEmpty()) {
            *error = trSftp("Download hash mismatch");
        }
        persistPartial();
        return false;
    }

    if (useFilepart) {
        if (QFile::exists(localPath) && !QFile::remove(localPath)) {
            if (error) {
                *error = trSftp("Cannot replace local file: %1").arg(localPath);
            }
            persistPartial();
            return false;
        }
        if (!QFile::rename(writePath, localPath)) {
            if (error) {
                *error = trSftp("Cannot finalize downloaded file: %1").arg(localPath);
            }
            persistPartial();
            return false;
        }
    }

    if (partialBytes) {
        *partialBytes = bytesDone;
    }
    if (partialSha256PrefixHex) {
        *partialSha256PrefixHex = streamedHex;
    }
    return true;
}
