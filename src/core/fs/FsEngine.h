/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "SftpTypes.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <functional>

#include <libssh/libssh.h>

/**
 * Protocol port for remote filesystem-over-SSH (SFTP, later SCP).
 * Parallel role to ITunnelSession: one interface, multiple engines.
 */
class FsEngine
{
public:
    enum Capability
    {
        List = 1 << 0,
        Mkdir = 1 << 1,
        Rename = 1 << 2,
        Remove = 1 << 3,
        Canonicalize = 1 << 4,
        Transfer = 1 << 5,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    using CancelCheck = std::function<bool(QString *error)>;
    using ProgressNote = std::function<void(qint64 bytesDelta, const QString &currentName)>;

    virtual ~FsEngine() = default;

    virtual Capabilities capabilities() const = 0;

    virtual bool open(ssh_session session, QString *failureMessage = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual bool
    listDirectoryEntries(const QString &path, QVector<RemoteEntry> *outEntries, QString *error) = 0;
    virtual bool createDirectory(const QString &path, QString *error) = 0;
    virtual bool renamePath(const QString &from, const QString &to, QString *error) = 0;
    virtual bool removeFile(const QString &path, QString *error) = 0;
    virtual bool removeDirectory(const QString &path, QString *error) = 0;
    virtual bool canonicalizePath(const QString &path, QString &canonicalOut, QString *error) = 0;
    virtual bool isRemoteDirectory(const QString &path, bool *isDir, QString *error) = 0;
    virtual bool remoteFileSize(const QString &path, qint64 *sizeOut, QString *error) = 0;

    virtual bool uploadFile(const QString &localPath,
                            const CancelCheck &shouldCancel,
                            const QString &remotePath,
                            const ProgressNote &onProgress,
                            QString *error) = 0;
    virtual bool downloadFile(const QString &remotePath,
                              const CancelCheck &shouldCancel,
                              const QString &localPath,
                              const ProgressNote &onProgress,
                              QString *error) = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FsEngine::Capabilities)
