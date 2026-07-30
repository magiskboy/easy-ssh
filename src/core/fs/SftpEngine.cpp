#include "SftpEngine.h"

#include <QCoreApplication>
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
    return List | Mkdir | Rename | Remove | Canonicalize | Transfer;
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

QString SftpEngine::formatPermissions(uint32_t permissions, uint8_t type)
{
    QString result(10, QLatin1Char('-'));

    switch (type) {
    case SSH_FILEXFER_TYPE_DIRECTORY:
        result[0] = QLatin1Char('d');
        break;
    case SSH_FILEXFER_TYPE_SYMLINK:
        result[0] = QLatin1Char('l');
        break;
    case SSH_FILEXFER_TYPE_SPECIAL:
        result[0] = QLatin1Char('s');
        break;
    default:
        break;
    }

    const auto setBit = [&](int index, uint32_t mask, QChar ch) {
        if (permissions & mask) {
            result[index] = ch;
        }
    };

    setBit(1, S_IRUSR, QLatin1Char('r'));
    setBit(2, S_IWUSR, QLatin1Char('w'));
    setBit(3, S_IXUSR, QLatin1Char('x'));
    setBit(4, S_IRGRP, QLatin1Char('r'));
    setBit(5, S_IWGRP, QLatin1Char('w'));
    setBit(6, S_IXGRP, QLatin1Char('x'));
    setBit(7, S_IROTH, QLatin1Char('r'));
    setBit(8, S_IWOTH, QLatin1Char('w'));
    setBit(9, S_IXOTH, QLatin1Char('x'));
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

bool SftpEngine::listDirectoryEntries(const QString &path,
                                      QVector<RemoteEntry> *outEntries,
                                      QString *error)
{
    const QByteArray remote = path.toUtf8();
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
        entry.name = name;
        entry.path = joinRemotePath(path, name);
        entry.isDir = attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
        entry.size = static_cast<qint64>(attributes->size);
        entry.permissions = formatPermissions(attributes->permissions, attributes->type);
        if (attributes->flags & SSH_FILEXFER_ATTR_ACMODTIME) {
            entry.mtime = static_cast<qint64>(attributes->mtime);
        } else if (attributes->mtime64 != 0) {
            entry.mtime = static_cast<qint64>(attributes->mtime64);
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
        if (a.isDir != b.isDir) {
            return a.isDir;
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

bool SftpEngine::canonicalizePath(const QString &path, QString *canonicalOut, QString *error)
{
    Q_UNUSED(error);
    const QString requested = path.isEmpty() ? QStringLiteral(".") : path;
    const QByteArray remote = requested.toUtf8();
    char *canonical = sftp_canonicalize_path(m_sftp, remote.constData());
    if (canonical == nullptr) {
        if (canonicalOut) {
            *canonicalOut = requested;
        }
        return true;
    }

    if (canonicalOut) {
        *canonicalOut = QString::fromUtf8(canonical);
    }
    ssh_string_free_char(canonical);
    return true;
}

bool SftpEngine::isRemoteDirectory(const QString &path, bool *isDir, QString *error)
{
    const QByteArray remote = path.toUtf8();
    sftp_attributes attributes = sftp_stat(m_sftp, remote.constData());
    if (attributes == nullptr) {
        if (error) {
            *error = trSftp("Cannot stat path: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    if (isDir) {
        *isDir = attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
    }
    sftp_attributes_free(attributes);
    return true;
}

bool SftpEngine::remoteFileSize(const QString &path, qint64 *sizeOut, QString *error)
{
    const QByteArray remote = path.toUtf8();
    sftp_attributes attrs = sftp_stat(m_sftp, remote.constData());
    if (attrs == nullptr) {
        if (error) {
            *error = trSftp("Cannot stat path: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    if (sizeOut) {
        *sizeOut = static_cast<qint64>(attrs->size);
    }
    sftp_attributes_free(attrs);
    return true;
}

bool SftpEngine::uploadFile(const QString &localPath,
                            const QString &remotePath,
                            const CancelCheck &shouldCancel,
                            const ProgressNote &onProgress,
                            QString *error)
{
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

    const QByteArray remote = remotePath.toUtf8();
    const int access = O_WRONLY | O_CREAT | O_TRUNC;
    sftp_file file =
        sftp_open(m_sftp, remote.constData(), access, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file == nullptr) {
        if (error) {
            *error = trSftp("Cannot open remote file for writing: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    const QString displayName = QFileInfo(localPath).fileName();
    if (onProgress) {
        onProgress(0, displayName);
    }

    char buffer[kXferBufSize];
    while (!local.atEnd()) {
        if (shouldCancel && shouldCancel(error)) {
            sftp_close(file);
            return false;
        }

        const qint64 nread = local.read(buffer, static_cast<qint64>(sizeof(buffer)));
        if (nread < 0) {
            if (error) {
                *error = trSftp("Cannot read local file: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
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
                sftp_close(file);
                return false;
            }

            const ssize_t nwritten = sftp_write(file, ptr, static_cast<size_t>(remaining));
            if (nwritten < 0) {
                if (error) {
                    *error = trSftp("Cannot write remote file: %1").arg(sftpErrorMessage());
                }
                sftp_close(file);
                return false;
            }
            ptr += nwritten;
            remaining -= nwritten;
            if (onProgress) {
                onProgress(nwritten, displayName);
            }
        }
    }

    if (sftp_close(file) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot close remote file: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SftpEngine::downloadFile(const QString &remotePath,
                              const QString &localPath,
                              const CancelCheck &shouldCancel,
                              const ProgressNote &onProgress,
                              QString *error)
{
    if (shouldCancel && shouldCancel(error)) {
        return false;
    }

    const QByteArray remote = remotePath.toUtf8();
    sftp_file file = sftp_open(m_sftp, remote.constData(), O_RDONLY, 0);
    if (file == nullptr) {
        if (error) {
            *error = trSftp("Cannot open remote file for reading: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
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

    char buffer[kXferBufSize];
    for (;;) {
        if (shouldCancel && shouldCancel(error)) {
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
            sftp_close(file);
            return false;
        }

        if (local.write(buffer, nbytes) != nbytes) {
            if (error) {
                *error = trSftp("Cannot write local file: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
            sftp_close(file);
            return false;
        }
        if (onProgress) {
            onProgress(nbytes, displayName);
        }
    }

    if (sftp_close(file) != SSH_OK) {
        if (error) {
            *error = trSftp("Cannot close remote file: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}
