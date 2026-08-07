// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ScpChunkTransfer.h"

#include "ScpEngine.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <cerrno>
#include <sys/stat.h>

#ifndef S_IRUSR
#define S_IRUSR 00400u
#define S_IWUSR 00200u
#define S_IRGRP 00040u
#define S_IROTH 00004u
#endif

namespace
{
constexpr size_t kChunkSize = 16384;
constexpr int kChunksPerTick = 4;

QString trScp(const char *text)
{
    return QCoreApplication::translate("ScpChunkTransfer", text);
}
} // namespace

ScpChunkTransfer::~ScpChunkTransfer()
{
    abort();
}

QString ScpChunkTransfer::sessionError() const
{
    if (m_session == nullptr) {
        return trScp("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : trScp("Unknown error");
}

QString ScpChunkTransfer::parentRemoteDir(const QString &remotePath)
{
    const int slash = remotePath.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return QStringLiteral(".");
    }
    if (slash == 0) {
        return QStringLiteral("/");
    }
    return remotePath.left(slash);
}

QString ScpChunkTransfer::remoteBaseName(const QString &remotePath)
{
    const int slash = remotePath.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return remotePath;
    }
    return remotePath.mid(slash + 1);
}

void ScpChunkTransfer::closeHandles()
{
    if (m_scp != nullptr) {
        ssh_scp_close(m_scp);
        ssh_scp_free(m_scp);
        m_scp = nullptr;
    }
    if (m_localFile.isOpen()) {
        m_localFile.close();
    }
}

void ScpChunkTransfer::abort()
{
    closeHandles();
    m_active = false;
    m_engine = nullptr;
    m_session = nullptr;
}

bool ScpChunkTransfer::startUpload(ScpEngine *engine,
                                   ssh_session session,
                                   const QString &localPath,
                                   const QString &remotePath,
                                   const TransferOptions &options,
                                   QString *error)
{
    abort();
    if (engine == nullptr || session == nullptr || !engine->isOpen()) {
        if (error) {
            *error = trScp("SCP is not available");
        }
        return false;
    }
    if (options.mode == TransferWriteMode::ResumeFilepart) {
        if (error) {
            *error = trScp("Resume is not supported over SCP");
        }
        return false;
    }

    m_engine = engine;
    m_session = session;
    m_kind = Kind::Upload;
    m_localPath = localPath;
    m_remotePath = remotePath;
    m_displayName = QFileInfo(localPath).fileName();
    m_bytesDone = 0;

    m_localFile.setFileName(localPath);
    if (!m_localFile.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = trScp("Cannot open local file: %1").arg(m_localFile.errorString());
        }
        return false;
    }

    m_bytesTotal = QFileInfo(localPath).size();
    const QString remoteDir = parentRemoteDir(remotePath);
    const QString remoteName = remoteBaseName(remotePath);
    const QByteArray location = remoteDir.toUtf8();

    m_scp = ssh_scp_new(session, SSH_SCP_WRITE, location.constData());
    if (m_scp == nullptr) {
        if (error) {
            *error = trScp("Failed to create SCP session: %1").arg(sessionError());
        }
        closeHandles();
        return false;
    }
    if (ssh_scp_init(m_scp) != SSH_OK) {
        if (error) {
            *error = trScp("Failed to initialize SCP: %1").arg(sessionError());
        }
        closeHandles();
        return false;
    }

    const QByteArray nameBytes = remoteName.toUtf8();
    if (ssh_scp_push_file64(m_scp,
                            nameBytes.constData(),
                            static_cast<uint64_t>(m_bytesTotal),
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != SSH_OK) {
        if (error) {
            *error = trScp("Cannot open remote file for writing: %1").arg(sessionError());
        }
        closeHandles();
        return false;
    }

    m_active = true;
    return true;
}

bool ScpChunkTransfer::startDownload(ScpEngine *engine,
                                     ssh_session session,
                                     const QString &remotePath,
                                     const QString &localPath,
                                     const TransferOptions &options,
                                     QString *error)
{
    abort();
    if (engine == nullptr || session == nullptr || !engine->isOpen()) {
        if (error) {
            *error = trScp("SCP is not available");
        }
        return false;
    }
    if (options.mode == TransferWriteMode::ResumeFilepart) {
        if (error) {
            *error = trScp("Resume is not supported over SCP");
        }
        return false;
    }

    m_engine = engine;
    m_session = session;
    m_kind = Kind::Download;
    m_localPath = localPath;
    m_remotePath = remotePath;
    m_displayName = QFileInfo(remotePath).fileName();
    m_bytesDone = 0;

    const QByteArray location = remotePath.toUtf8();
    m_scp = ssh_scp_new(session, SSH_SCP_READ, location.constData());
    if (m_scp == nullptr) {
        if (error) {
            *error = trScp("Failed to create SCP session: %1").arg(sessionError());
        }
        return false;
    }
    if (ssh_scp_init(m_scp) != SSH_OK) {
        if (error) {
            *error = trScp("Failed to initialize SCP: %1").arg(sessionError());
        }
        closeHandles();
        return false;
    }

    const int req = ssh_scp_pull_request(m_scp);
    if (req != SSH_SCP_REQUEST_NEWFILE) {
        if (error) {
            if (req == SSH_SCP_REQUEST_WARNING) {
                const char *warn = ssh_scp_request_get_warning(m_scp);
                *error = warn ? QString::fromUtf8(warn) : trScp("SCP pull warning");
            } else {
                *error = trScp("Unexpected SCP pull response: %1").arg(sessionError());
            }
        }
        closeHandles();
        return false;
    }

    m_bytesTotal = static_cast<qint64>(ssh_scp_request_get_size64(m_scp));
    m_remaining = ssh_scp_request_get_size64(m_scp);

    if (ssh_scp_accept_request(m_scp) != SSH_OK) {
        if (error) {
            *error = trScp("Cannot accept SCP file: %1").arg(sessionError());
        }
        closeHandles();
        return false;
    }

    m_localFile.setFileName(localPath);
    if (!m_localFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = (errno == ENOSPC) ? trScp("Disk full")
                                       : trScp("Cannot open local file for writing: %1")
                                             .arg(m_localFile.errorString());
        }
        closeHandles();
        return false;
    }

    m_active = true;
    return true;
}

ScpChunkTransfer::TickResult ScpChunkTransfer::tick(const FsEngine::CancelCheck &shouldCancel,
                                                    const FsEngine::ProgressNote &onProgress,
                                                    QString *error)
{
    if (!m_active || m_scp == nullptr) {
        return TickResult::Failed;
    }

    char buffer[kChunkSize];
    for (int i = 0; i < kChunksPerTick; ++i) {
        if (shouldCancel && shouldCancel(error)) {
            abort();
            return TickResult::Failed;
        }

        if (m_kind == Kind::Upload) {
            if (m_localFile.atEnd()) {
                closeHandles();
                m_active = false;
                return TickResult::Done;
            }
            const qint64 nread = m_localFile.read(buffer, static_cast<qint64>(sizeof(buffer)));
            if (nread < 0) {
                if (error) {
                    *error = trScp("Cannot read local file: %1").arg(m_localFile.errorString());
                }
                abort();
                return TickResult::Failed;
            }
            if (nread == 0) {
                closeHandles();
                m_active = false;
                return TickResult::Done;
            }
            if (ssh_scp_write(m_scp, buffer, static_cast<size_t>(nread)) != SSH_OK) {
                if (error) {
                    *error = trScp("Cannot write remote file: %1").arg(sessionError());
                }
                abort();
                return TickResult::Failed;
            }
            m_bytesDone += nread;
            if (onProgress) {
                onProgress(nread, m_displayName);
            }
        } else {
            if (m_remaining == 0) {
                closeHandles();
                m_active = false;
                return TickResult::Done;
            }
            const size_t toRead = static_cast<size_t>(qMin<uint64_t>(m_remaining, sizeof(buffer)));
            const int nbytes = ssh_scp_read(m_scp, buffer, toRead);
            if (nbytes == SSH_ERROR || nbytes < 0) {
                if (error) {
                    *error = trScp("Cannot read remote file: %1").arg(sessionError());
                }
                abort();
                return TickResult::Failed;
            }
            if (nbytes == 0) {
                closeHandles();
                m_active = false;
                return TickResult::Done;
            }
            if (m_localFile.write(buffer, nbytes) != nbytes) {
                if (error) {
                    *error =
                        (errno == ENOSPC)
                            ? trScp("Disk full")
                            : trScp("Cannot write local file: %1").arg(m_localFile.errorString());
                }
                abort();
                return TickResult::Failed;
            }
            m_remaining -= static_cast<uint64_t>(nbytes);
            m_bytesDone += nbytes;
            if (onProgress) {
                onProgress(nbytes, m_displayName);
            }
        }
    }
    return TickResult::Again;
}
