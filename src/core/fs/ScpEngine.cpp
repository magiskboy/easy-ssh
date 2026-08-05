// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ScpEngine.h"

#include "Symlink.h"
#include "TransferTypes.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>

#include <cerrno>
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

QString trScp(const char *text)
{
    return QCoreApplication::translate("ScpEngine", text);
}
} // namespace

ScpEngine::ScpEngine(const ShellCommandSetConfig &config) : m_commands(config) {}

ScpEngine::~ScpEngine()
{
    close();
}

void ScpEngine::setCommandConfig(const ShellCommandSetConfig &config)
{
    m_commands.setConfig(config);
    if (m_runner) {
        m_runner->setShellPath(config.shell);
    }
}

FsEngine::Capabilities ScpEngine::capabilities() const
{
    return List | Mkdir | Rename | Remove | Canonicalize | Transfer | Symlink;
}

QString ScpEngine::sessionError() const
{
    if (m_session == nullptr) {
        return trScp("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : trScp("Unknown error");
}

QString ScpEngine::localIoErrorMessage(const QString &qtErrorString)
{
    if (errno == ENOSPC) {
        return trScp("Disk full");
    }
    return qtErrorString;
}

QString ScpEngine::parentRemoteDir(const QString &remotePath)
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

QString ScpEngine::remoteBaseName(const QString &remotePath)
{
    const int slash = remotePath.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return remotePath;
    }
    return remotePath.mid(slash + 1);
}

QString ScpEngine::lsOptions() const
{
    if (m_fullTimeOk) {
        return QStringLiteral("--full-time");
    }
    return {};
}

bool ScpEngine::runChecked(const QString &command,
                           ShellExecRunner::Result *result,
                           QString *error,
                           bool allowExitOneWithStdout)
{
    if (!m_runner) {
        if (error) {
            *error = trScp("Remote FS is not available");
        }
        return false;
    }

    ShellExecRunner::Result local;
    ShellExecRunner::Result *out = result ? result : &local;
    if (!m_runner->run(command, out, error)) {
        return false;
    }

    const bool okExit = (out->exitStatus == 0) || (allowExitOneWithStdout && out->exitStatus == 1 &&
                                                   !out->stdoutBytes.isEmpty());
    if (!okExit) {
        QString detail = ShellExecRunner::stderrText(*out).trimmed();
        if (detail.isEmpty()) {
            detail = ShellExecRunner::stdoutText(*out).trimmed();
        }
        if (error) {
            if (detail.isEmpty()) {
                *error = trScp("Remote command failed (exit %1)").arg(out->exitStatus);
            } else {
                *error = detail;
            }
        }
        return false;
    }
    return true;
}

bool ScpEngine::probeScp(QString *failureMessage)
{
    ssh_scp scp = ssh_scp_new(m_session, SSH_SCP_WRITE, ".");
    if (scp == nullptr) {
        if (failureMessage) {
            *failureMessage = trScp("Failed to create SCP session: %1").arg(sessionError());
        }
        return false;
    }
    const int rc = ssh_scp_init(scp);
    ssh_scp_close(scp);
    ssh_scp_free(scp);
    if (rc != SSH_OK) {
        if (failureMessage) {
            *failureMessage = trScp("SCP is unavailable: %1").arg(sessionError());
        }
        return false;
    }
    return true;
}

bool ScpEngine::open(ssh_session session, QString *failureMessage)
{
    close();
    m_session = session;
    m_runner = std::make_unique<ShellExecRunner>(session, m_commands.config().shell);
    m_fullTimeOk = false;
    m_fullTimeProbed = false;

    const QString startup = m_commands.formatStartupCommands();
    if (!startup.isEmpty()) {
        ShellExecRunner::Result ignored;
        m_runner->run(startup, &ignored, nullptr); // soft-fail like WinSCP
    }

    ShellExecRunner::Result pwdResult;
    QString pwdError;
    if (!runChecked(m_commands.formatPwd(), &pwdResult, &pwdError)) {
        if (failureMessage) {
            *failureMessage = trScp("Shell remote FS probe failed: %1")
                                  .arg(pwdError.isEmpty() ? sessionError() : pwdError);
        }
        close();
        return false;
    }

    QString scpError;
    if (!probeScp(&scpError)) {
        if (failureMessage) {
            *failureMessage = scpError;
        }
        close();
        return false;
    }

    return true;
}

void ScpEngine::close()
{
    m_runner.reset();
    m_session = nullptr;
    m_fullTimeOk = false;
    m_fullTimeProbed = false;
}

bool ScpEngine::listDirectoryEntries(const QString &path,
                                     QVector<RemoteEntry> *outEntries,
                                     QString *error)
{
    const QString listPath = Symlink::directoryListPath(path);

    if (!m_commands.config().tryFullTime) {
        m_fullTimeProbed = true;
        m_fullTimeOk = false;
    } else if (!m_fullTimeProbed) {
        m_fullTimeProbed = true;
        ShellExecRunner::Result probe;
        const QString cmd = m_commands.formatListDirectory(listPath, QStringLiteral("--full-time"));
        if (m_runner->run(cmd, &probe, nullptr) && probe.exitStatus == 0) {
            m_fullTimeOk = true;
            if (!ShellCommandSet::parseLsListing(
                    ShellExecRunner::stdoutText(probe), outEntries, path, error)) {
                return false;
            }
            annotateSymlinkTargets(outEntries);
            return true;
        }
        m_fullTimeOk = false;
    }

    ShellExecRunner::Result result;
    const bool allowWarn = m_commands.config().ignoreLsWarnings;
    if (!runChecked(
            m_commands.formatListDirectory(listPath, lsOptions()), &result, error, allowWarn)) {
        return false;
    }
    if (!ShellCommandSet::parseLsListing(
            ShellExecRunner::stdoutText(result), outEntries, path, error)) {
        return false;
    }
    annotateSymlinkTargets(outEntries);
    return true;
}

void ScpEngine::annotateSymlinkTargets(QVector<RemoteEntry> *entries)
{
    if (!entries) {
        return;
    }
    for (RemoteEntry &entry : *entries) {
        if (!entry.isSymlink) {
            continue;
        }
        entry.linkIsDir = runChecked(m_commands.formatTestDirectory(entry.path), nullptr, nullptr);
    }
}

bool ScpEngine::createDirectory(const QString &path, QString *error)
{
    return runChecked(m_commands.formatMkdir(path), nullptr, error);
}

bool ScpEngine::renamePath(const QString &from, const QString &to, QString *error)
{
    return runChecked(m_commands.formatRename(from, to), nullptr, error);
}

bool ScpEngine::removeFile(const QString &path, QString *error)
{
    return runChecked(m_commands.formatRemove(path), nullptr, error);
}

bool ScpEngine::removeDirectory(const QString &path, QString *error)
{
    return runChecked(m_commands.formatRemove(path), nullptr, error);
}

bool ScpEngine::canonicalizePath(const QString &path, QString &canonicalOut, QString *error)
{
    ShellExecRunner::Result result;
    if (runChecked(m_commands.formatRealpath(path), &result, nullptr)) {
        canonicalOut = ShellExecRunner::stdoutText(result).trimmed();
        if (!canonicalOut.isEmpty()) {
            return true;
        }
    }

    // Fallback: cd into path && pwd
    const QString fallback = QStringLiteral("cd %1 && pwd").arg(ShellCommandSet::shellQuote(path));
    if (!runChecked(fallback, &result, error)) {
        return false;
    }
    canonicalOut = ShellExecRunner::stdoutText(result).trimmed();
    if (canonicalOut.isEmpty()) {
        if (error) {
            *error = trScp("Cannot resolve remote path");
        }
        return false;
    }
    return true;
}

bool ScpEngine::statEntry(const QString &path, RemoteEntry *out, bool follow, QString *error)
{
    if (!out) {
        if (error) {
            *error = trScp("Internal error: missing entry");
        }
        return false;
    }

    if (follow) {
        ShellExecRunner::Result testResult;
        const bool isDir = runChecked(m_commands.formatTestDirectory(path), &testResult, nullptr);
        ShellExecRunner::Result result;
        const bool allowWarn = m_commands.config().ignoreLsWarnings;
        if (!runChecked(m_commands.formatListFile(path, lsOptions()), &result, error, allowWarn)) {
            return false;
        }
        if (!ShellCommandSet::parseLsSingle(
                ShellExecRunner::stdoutText(result), out, path, error)) {
            return false;
        }
        // Followed view: directory-ness from test -d; drop symlink leaf flag.
        out->isDir = isDir;
        if (isDir) {
            out->isSymlink = false;
            out->linkTarget.clear();
        } else if (out->isSymlink) {
            // Symlink to non-dir (or broken): still a symlink leaf for preserve semantics,
            // but resolveEntry treats isDir=false.
            out->isDir = false;
        }
        return true;
    }

    ShellExecRunner::Result result;
    const bool allowWarn = m_commands.config().ignoreLsWarnings;
    if (!runChecked(m_commands.formatListFile(path, lsOptions()), &result, error, allowWarn)) {
        return false;
    }
    return ShellCommandSet::parseLsSingle(ShellExecRunner::stdoutText(result), out, path, error);
}

bool ScpEngine::isRemoteDirectory(const QString &path, bool *isDir, QString *error)
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

bool ScpEngine::remoteFileSize(const QString &path, qint64 *sizeOut, QString *error)
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

bool ScpEngine::createSymlink(const QString &target, const QString &linkPath, QString *error)
{
    return runChecked(m_commands.formatSymlink(target, linkPath), nullptr, error);
}

bool ScpEngine::readSymlink(const QString &path, QString &targetOut, QString *error)
{
    ShellExecRunner::Result result;
    if (!runChecked(m_commands.formatReadlink(path), &result, error)) {
        return false;
    }
    targetOut = Symlink::normalizeTarget(ShellExecRunner::stdoutText(result));
    return true;
}

bool ScpEngine::uploadFile(const QString &localPath,
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
    if (options.mode == TransferWriteMode::ResumeFilepart) {
        if (error) {
            *error = trScp("Resume is not supported over SCP");
        }
        return false;
    }

    if (shouldCancel && shouldCancel(error)) {
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        if (error) {
            *error =
                trScp("Cannot open local file: %1").arg(localIoErrorMessage(local.errorString()));
        }
        return false;
    }

    const QFileInfo info(localPath);
    const qint64 fileSize = info.size();
    const QString remoteDir = parentRemoteDir(remotePath);
    const QString remoteName = remoteBaseName(remotePath);
    const QByteArray location = remoteDir.toUtf8();

    ssh_scp scp = ssh_scp_new(m_session, SSH_SCP_WRITE, location.constData());
    if (scp == nullptr) {
        if (error) {
            *error = trScp("Failed to create SCP session: %1").arg(sessionError());
        }
        return false;
    }

    auto freeScp = [&]() {
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        scp = nullptr;
    };

    if (ssh_scp_init(scp) != SSH_OK) {
        if (error) {
            *error = trScp("Failed to initialize SCP: %1").arg(sessionError());
        }
        freeScp();
        return false;
    }

    const QByteArray nameBytes = remoteName.toUtf8();
    if (ssh_scp_push_file64(scp,
                            nameBytes.constData(),
                            static_cast<uint64_t>(fileSize),
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != SSH_OK) {
        if (error) {
            *error = trScp("Cannot open remote file for writing: %1").arg(sessionError());
        }
        freeScp();
        return false;
    }

    const QString displayName = info.fileName();
    if (onProgress) {
        onProgress(0, displayName);
    }

    char buffer[kXferBufSize];
    while (!local.atEnd()) {
        if (shouldCancel && shouldCancel(error)) {
            freeScp();
            return false;
        }

        const qint64 nread = local.read(buffer, static_cast<qint64>(sizeof(buffer)));
        if (nread < 0) {
            if (error) {
                *error = trScp("Cannot read local file: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
            freeScp();
            return false;
        }
        if (nread == 0) {
            break;
        }

        if (ssh_scp_write(scp, buffer, static_cast<size_t>(nread)) != SSH_OK) {
            if (error) {
                *error = trScp("Cannot write remote file: %1").arg(sessionError());
            }
            freeScp();
            return false;
        }
        if (onProgress) {
            onProgress(nread, displayName);
        }
    }

    freeScp();
    return true;
}

bool ScpEngine::downloadFile(const QString &remotePath,
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
    if (options.mode == TransferWriteMode::ResumeFilepart) {
        if (error) {
            *error = trScp("Resume is not supported over SCP");
        }
        return false;
    }

    if (shouldCancel && shouldCancel(error)) {
        return false;
    }

    const QByteArray location = remotePath.toUtf8();
    ssh_scp scp = ssh_scp_new(m_session, SSH_SCP_READ, location.constData());
    if (scp == nullptr) {
        if (error) {
            *error = trScp("Failed to create SCP session: %1").arg(sessionError());
        }
        return false;
    }

    auto freeScp = [&]() {
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        scp = nullptr;
    };

    if (ssh_scp_init(scp) != SSH_OK) {
        if (error) {
            *error = trScp("Failed to initialize SCP: %1").arg(sessionError());
        }
        freeScp();
        return false;
    }

    const int req = ssh_scp_pull_request(scp);
    if (req != SSH_SCP_REQUEST_NEWFILE) {
        if (error) {
            if (req == SSH_SCP_REQUEST_WARNING) {
                const char *warn = ssh_scp_request_get_warning(scp);
                *error = warn ? QString::fromUtf8(warn) : trScp("SCP pull warning");
            } else {
                *error = trScp("Unexpected SCP pull response: %1").arg(sessionError());
            }
        }
        freeScp();
        return false;
    }

    const uint64_t remoteSize = ssh_scp_request_get_size64(scp);
    Q_UNUSED(remoteSize);

    if (ssh_scp_accept_request(scp) != SSH_OK) {
        if (error) {
            *error = trScp("Cannot accept SCP file: %1").arg(sessionError());
        }
        freeScp();
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = trScp("Cannot open local file for writing: %1")
                         .arg(localIoErrorMessage(local.errorString()));
        }
        freeScp();
        return false;
    }

    const QString displayName = QFileInfo(remotePath).fileName();
    if (onProgress) {
        onProgress(0, displayName);
    }

    char buffer[kXferBufSize];
    uint64_t remaining = remoteSize;
    while (remaining > 0) {
        if (shouldCancel && shouldCancel(error)) {
            freeScp();
            return false;
        }

        const size_t toRead = static_cast<size_t>(qMin<uint64_t>(remaining, sizeof(buffer)));
        const int nbytes = ssh_scp_read(scp, buffer, toRead);
        if (nbytes == SSH_ERROR || nbytes < 0) {
            if (error) {
                *error = trScp("Cannot read remote file: %1").arg(sessionError());
            }
            freeScp();
            return false;
        }
        if (nbytes == 0) {
            break;
        }
        if (local.write(buffer, nbytes) != nbytes) {
            if (error) {
                *error = trScp("Cannot write local file: %1")
                             .arg(localIoErrorMessage(local.errorString()));
            }
            freeScp();
            return false;
        }
        remaining -= static_cast<uint64_t>(nbytes);
        if (onProgress) {
            onProgress(nbytes, displayName);
        }
    }

    freeScp();
    return true;
}
