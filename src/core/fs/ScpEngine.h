#pragma once

#include "FsEngine.h"

/**
 * SCP engine — not implement yet. All operations return NotSupported.
 * Future: native SCP transfer; list/CRUD remain NotSupported or workaround.
 */
class ScpEngine final : public FsEngine
{
public:
    Capabilities capabilities() const override { return {}; }

    bool open(ssh_session session, QString *failureMessage = nullptr) override;
    void close() override;
    bool isOpen() const override { return false; }

    bool listDirectoryEntries(const QString &path,
                              QVector<RemoteEntry> *outEntries,
                              QString *error) override;
    bool createDirectory(const QString &path, QString *error) override;
    bool renamePath(const QString &from, const QString &to, QString *error) override;
    bool removeFile(const QString &path, QString *error) override;
    bool removeDirectory(const QString &path, QString *error) override;
    bool canonicalizePath(const QString &path, QString *canonicalOut, QString *error) override;
    bool isRemoteDirectory(const QString &path, bool *isDir, QString *error) override;
    bool remoteFileSize(const QString &path, qint64 *sizeOut, QString *error) override;

    bool uploadFile(const QString &localPath,
                    const QString &remotePath,
                    const CancelCheck &shouldCancel,
                    const ProgressNote &onProgress,
                    QString *error) override;
    bool downloadFile(const QString &remotePath,
                      const QString &localPath,
                      const CancelCheck &shouldCancel,
                      const ProgressNote &onProgress,
                      QString *error) override;

private:
    static bool notSupported(QString *error);
};
