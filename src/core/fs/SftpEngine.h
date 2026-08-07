/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "FsEngine.h"

#include <QCryptographicHash>
#include <QFile>
#include <QVector>

#include <libssh/sftp.h>

class SftpAioTransfer;

/**
 * SFTP implementation of FsEngine. Migrated from the protocol layer of SftpClient.
 */
class SftpEngine final : public FsEngine
{
public:
    friend class SftpAioTransfer;

    SftpEngine() = default;
    ~SftpEngine() override;

    SftpEngine(const SftpEngine &) = delete;
    SftpEngine &operator=(const SftpEngine &) = delete;

    Capabilities capabilities() const override;

    bool open(ssh_session session, QString *failureMessage = nullptr) override;
    void close() override;
    bool isOpen() const override { return m_sftp != nullptr; }
    sftp_session handle() const { return m_sftp; }

    bool listDirectoryEntries(const QString &path,
                              QVector<RemoteEntry> *outEntries,
                              QString *error) override;
    bool createDirectory(const QString &path, QString *error) override;
    bool renamePath(const QString &from, const QString &to, QString *error) override;
    bool removeFile(const QString &path, QString *error) override;
    bool removeDirectory(const QString &path, QString *error) override;
    bool canonicalizePath(const QString &path, QString &canonicalOut, QString *error) override;
    bool statEntry(const QString &path, RemoteEntry *out, bool follow, QString *error) override;
    bool isRemoteDirectory(const QString &path, bool *isDir, QString *error) override;
    bool remoteFileSize(const QString &path, qint64 *sizeOut, QString *error) override;
    bool createSymlink(const QString &target, const QString &linkPath, QString *error) override;
    bool readSymlink(const QString &path, QString &targetOut, QString *error) override;

    bool uploadFile(const QString &localPath,
                    const CancelCheck &shouldCancel,
                    const QString &remotePath,
                    const ProgressNote &onProgress,
                    const TransferOptions &options,
                    QString *error,
                    qint64 *partialBytes = nullptr,
                    QString *partialSha256PrefixHex = nullptr) override;
    bool downloadFile(const QString &remotePath,
                      const CancelCheck &shouldCancel,
                      const QString &localPath,
                      const ProgressNote &onProgress,
                      const TransferOptions &options,
                      QString *error,
                      qint64 *partialBytes = nullptr,
                      QString *partialSha256PrefixHex = nullptr) override;

    /**
     * Chunked directory listing for IoLoop (Phase 3). Caller runs brief blocking
     * around open/readBatch/close; yields between batches.
     */
    class DirListSession
    {
    public:
        DirListSession() = default;
        ~DirListSession();
        DirListSession(const DirListSession &) = delete;
        DirListSession &operator=(const DirListSession &) = delete;

        bool open(SftpEngine *engine, const QString &path, QString *error);
        /// Append up to @p maxEntries (excluding . / ..). Sets *eof when finished.
        bool readBatch(int maxEntries, QVector<RemoteEntry> *outEntries, bool *eof, QString *error);
        void close();
        bool isOpen() const { return m_dir != nullptr; }
        QString path() const { return m_path; }

    private:
        SftpEngine *m_engine = nullptr;
        sftp_dir m_dir = nullptr;
        QString m_path;
    };

private:
    enum class EntryType : uint8_t
    {
        Regular = SSH_FILEXFER_TYPE_REGULAR,
        Directory = SSH_FILEXFER_TYPE_DIRECTORY,
        Symlink = SSH_FILEXFER_TYPE_SYMLINK,
        Special = SSH_FILEXFER_TYPE_SPECIAL,
    };

    QString sessionErrorOf(ssh_session session) const;
    QString sftpErrorMessage() const;
    static QString localIoErrorMessage(const QString &qtErrorString);
    static QString formatPermissions(uint32_t permissions, EntryType type);
    static QString joinRemotePath(const QString &dir, const QString &name);
    static void fillEntryFromAttributes(RemoteEntry *entry,
                                        const sftp_attributes attributes,
                                        const QString &path,
                                        const QString &name);
    QString readlinkAt(const QString &path) const;
    /// Path to pass to sftp_opendir; follows symlink-to-dir via canonicalize.
    QString directoryOpenPath(const QString &path);

    bool
    hashLocalPrefix(const QString &localPath, qint64 length, QString &hexOut, QString *error) const;
    bool hashRemotePrefix(const QString &remotePath,
                          qint64 length,
                          QString &hexOut,
                          QString *error) const;
    bool hashLocalFile(const QString &localPath, QString &hexOut, QString *error) const;
    bool
    feedHashFromLocal(QFile &local, qint64 length, QCryptographicHash *hash, QString *error) const;

    sftp_session m_sftp = nullptr;
    ssh_session m_session = nullptr;
};
