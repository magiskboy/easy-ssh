/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "FsEngine.h"
#include "ShellCommandSet.h"
#include "ShellExecRunner.h"
#include "core/connection/Connection.h"

#include <memory>

/**
 * SCP + shell remote FS (WinSCP-style): shell CommandSet for browse/CRUD,
 * libssh SCP for file transfer.
 */
class ScpEngine final : public FsEngine
{
public:
    ScpEngine() = default;
    explicit ScpEngine(const ShellCommandSetConfig &config);
    ~ScpEngine() override;

    ScpEngine(const ScpEngine &) = delete;
    ScpEngine &operator=(const ScpEngine &) = delete;

    void setCommandConfig(const ShellCommandSetConfig &config);

    Capabilities capabilities() const override;

    bool open(ssh_session session, QString *failureMessage = nullptr) override;
    void close() override;
    bool isOpen() const override { return m_session != nullptr && m_runner != nullptr; }

    bool listDirectoryEntries(const QString &path,
                              QVector<RemoteEntry> *outEntries,
                              QString *error) override;
    bool createDirectory(const QString &path, QString *error) override;
    bool renamePath(const QString &from, const QString &to, QString *error) override;
    bool removeFile(const QString &path, QString *error) override;
    bool removeDirectory(const QString &path, QString *error) override;
    bool canonicalizePath(const QString &path, QString &canonicalOut, QString *error) override;
    bool isRemoteDirectory(const QString &path, bool *isDir, QString *error) override;
    bool remoteFileSize(const QString &path, qint64 *sizeOut, QString *error) override;

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

private:
    QString sessionError() const;
    QString lsOptions() const;
    bool runChecked(const QString &command,
                    ShellExecRunner::Result *result,
                    QString *error,
                    bool allowExitOneWithStdout = false);
    bool statEntry(const QString &path, RemoteEntry *out, QString *error);
    bool probeScp(QString *failureMessage);
    static QString parentRemoteDir(const QString &remotePath);
    static QString remoteBaseName(const QString &remotePath);
    static QString localIoErrorMessage(const QString &qtErrorString);

    ShellCommandSet m_commands;
    ssh_session m_session = nullptr;
    std::unique_ptr<ShellExecRunner> m_runner;
    bool m_fullTimeOk = false;
    bool m_fullTimeProbed = false;
};
