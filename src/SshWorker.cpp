#include "SshWorker.h"

#include "Logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QSaveFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>

namespace
{
constexpr size_t kXferBufSize = 16384;
constexpr qint64 kProgressEmitBytes = 64 * 1024;
constexpr qint64 kProgressEmitMs = 100;

QString joinRemotePath(const QString &dir, const QString &name)
{
    if (dir.isEmpty() || dir == QLatin1String(".")) {
        return name;
    }
    if (dir.endsWith(QLatin1Char('/'))) {
        return dir + name;
    }
    return dir + QLatin1Char('/') + name;
}
} // namespace

SshWorker::SshWorker(QObject *parent) : QObject(parent) {}

SshWorker::~SshWorker()
{
    cleanup();
}

void SshWorker::connectToHost(const Connection &connection,
                              const QString &secret,
                              int cols,
                              int rows)
{
    if (m_running) {
        emit errorOccurred(tr("Session already connected"));
        return;
    }

    cleanup();

    qCWarning(lcSsh) << "Connecting to" << connection.username + QLatin1Char('@') + connection.host
                     << "port" << connection.port;

    m_ptyCols = qBound(2, cols, 1000);
    m_ptyRows = qBound(2, rows, 500);

    m_session = ssh_new();
    if (m_session == nullptr) {
        emit errorOccurred(tr("Failed to create SSH session"));
        return;
    }

    if (connection.source == ConnectionSource::SshConfig && !connection.configAlias.isEmpty()) {
        // Match OpenSSH: set Host to the config alias, then let libssh apply ~/.ssh/config
        // (including Include / HostName / User / Port / IdentityFile / ProxyJump).
        const QByteArray alias = connection.configAlias.toUtf8();
        ssh_options_set(m_session, SSH_OPTIONS_HOST, alias.constData());
        if (ssh_options_parse_config(m_session, nullptr) != SSH_OK) {
            qCWarning(lcSsh) << "ssh_options_parse_config failed for alias" << connection.configAlias
                             << ":" << sessionError();
        }
    } else {
        const QByteArray host = connection.host.toUtf8();
        const QByteArray user = connection.username.toUtf8();
        int port = connection.port;

        ssh_options_set(m_session, SSH_OPTIONS_HOST, host.constData());
        ssh_options_set(m_session, SSH_OPTIONS_USER, user.constData());
        ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);

        // Keep app-defined host/user/port authoritative.
        const int processConfig = 0;
        ssh_options_set(m_session, SSH_OPTIONS_PROCESS_CONFIG, &processConfig);
    }

    if (ssh_connect(m_session) != SSH_OK) {
        const QString err = sessionError();
        qCWarning(lcSsh) << "Connection failed:" << err;
        cleanup();
        emit errorOccurred(tr("Connection failed: %1").arg(err));
        return;
    }

    if (!verifyKnownHost()) {
        cleanup();
        return;
    }

    QString mutableSecret = secret;
    if (!authenticate(connection, mutableSecret)) {
        qCWarning(lcSsh) << "Authentication failed for" << connection.host;
        mutableSecret.fill(QChar(u'\0'));
        cleanup();
        return;
    }
    mutableSecret.fill(QChar(u'\0'));

    if (!openShell()) {
        cleanup();
        return;
    }

    // SFTP is optional — shell stays usable when the server rejects the subsystem.
    QString sftpFailure;
    const bool sftpReady = openSftp(&sftpFailure);

    m_running = true;
    qCWarning(lcSsh) << "Connected to" << connection.host << "sftp:" << (sftpReady ? "yes" : "no");
    emit connected();

    if (!sftpReady) {
        qCWarning(lcSsh) << "SFTP unavailable:" << sftpFailure;
        emit sftpUnavailable(sftpFailure);
    }

    if (m_ioTimer == nullptr) {
        m_ioTimer = new QTimer(this);
        connect(m_ioTimer, &QTimer::timeout, this, &SshWorker::pollChannel);
    }
    m_ioTimer->start(20);
}

void SshWorker::writeToChannel(const QByteArray &data)
{
    if (!m_running || m_channel == nullptr || data.isEmpty()) {
        return;
    }

    const char *ptr = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        const int written = ssh_channel_write(m_channel, ptr, static_cast<uint32_t>(remaining));
        if (written == SSH_ERROR || written < 0) {
            if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel) ||
                (m_session && !ssh_is_connected(m_session))) {
                disconnectSession();
                return;
            }
            emit errorOccurred(tr("Failed to write to channel: %1").arg(sessionError()));
            disconnectSession();
            return;
        }
        if (written == 0) {
            break;
        }
        ptr += written;
        remaining -= written;
    }
}

void SshWorker::changePtySize(int cols, int rows)
{
    if (!m_running || m_channel == nullptr) {
        return;
    }
    // Match Konsole/QTermWidget thresholds — reject transient 0/1 sizes during maximize.
    if (cols < 2 || rows < 2) {
        return;
    }
    if (cols > 1000 || rows > 500) {
        return;
    }

    if (cols == m_ptyCols && rows == m_ptyRows) {
        return;
    }

    if (ssh_channel_change_pty_size(m_channel, cols, rows) != SSH_OK) {
        // Non-fatal: keep the session; size may sync on the next resize.
        return;
    }

    m_ptyCols = cols;
    m_ptyRows = rows;
}

void SshWorker::disconnectSession()
{
    if (m_ioTimer) {
        m_ioTimer->stop();
    }

    const bool wasRunning = m_running;
    cleanup();

    if (wasRunning) {
        qCWarning(lcSsh) << "Session disconnected";
        emit disconnected();
    }
}

void SshWorker::respondHostKeyTrust(bool accept)
{
    QMutexLocker locker(&m_hostKeyMutex);
    m_hostKeyAccepted = accept;
    m_hostKeyAnswered = true;
    m_hostKeyCondition.wakeAll();
}

void SshWorker::listDirectory(const QString &path)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QVector<RemoteEntry> entries;
    QString error;
    if (!listDirectoryEntries(path, &entries, &error)) {
        emit sftpError(error);
        return;
    }

    emit directoryListed(path, entries);
}

void SshWorker::createDirectory(const QString &path)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    const QByteArray remote = path.toUtf8();
    if (sftp_mkdir(m_sftp, remote.constData(), S_IRWXU) != SSH_OK) {
        emit sftpError(tr("Cannot create folder: %1").arg(sftpErrorMessage()));
        return;
    }

    emit sftpFinished(tr("Created folder: %1").arg(path));
}

void SshWorker::renamePath(const QString &from, const QString &to)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    const QByteArray src = from.toUtf8();
    const QByteArray dst = to.toUtf8();
    if (sftp_rename(m_sftp, src.constData(), dst.constData()) != SSH_OK) {
        emit sftpError(tr("Cannot rename: %1").arg(sftpErrorMessage()));
        return;
    }

    emit sftpFinished(tr("Renamed to %1").arg(QFileInfo(to).fileName()));
}

void SshWorker::removePath(const QString &path, bool recursive)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    bool isDir = false;
    QString error;
    if (!isRemoteDirectory(path, &isDir, &error)) {
        emit sftpError(error);
        return;
    }

    if (isDir) {
        if (recursive) {
            if (!removePathRecursive(path, &error)) {
                emit sftpError(error);
                return;
            }
        } else {
            const QByteArray remote = path.toUtf8();
            if (sftp_rmdir(m_sftp, remote.constData()) != SSH_OK) {
                emit sftpError(tr("Cannot delete folder: %1").arg(sftpErrorMessage()));
                return;
            }
        }
    } else {
        const QByteArray remote = path.toUtf8();
        if (sftp_unlink(m_sftp, remote.constData()) != SSH_OK) {
            emit sftpError(tr("Cannot delete file: %1").arg(sftpErrorMessage()));
            return;
        }
    }

    emit sftpFinished(tr("Deleted: %1").arg(path));
}

void SshWorker::uploadFiles(const QStringList &localPaths, const QString &remoteDir)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    beginTransfer(computeLocalBytes(localPaths));

    QString error;
    for (const QString &localPath : localPaths) {
        if (transferCanceled(&error)) {
            endTransfer();
            emit sftpCanceled(error);
            return;
        }

        const QFileInfo info(localPath);
        if (!info.exists()) {
            endTransfer();
            emit sftpError(tr("Local path does not exist: %1").arg(localPath));
            return;
        }

        const QString remotePath = joinRemotePath(remoteDir, info.fileName());
        if (!uploadPathRecursive(localPath, remotePath, &error)) {
            endTransfer();
            if (m_transferCancel.load(std::memory_order_relaxed)) {
                emit sftpCanceled(error);
            } else {
                emit sftpError(error);
            }
            return;
        }
    }

    endTransfer();
    emit sftpFinished(tr("Upload finished (%1 item(s))").arg(localPaths.size()));
}

void SshWorker::uploadFileTo(const QString &localPath, const QString &remotePath)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    const QFileInfo info(localPath);
    if (!info.exists() || !info.isFile()) {
        emit sftpError(tr("Local file does not exist: %1").arg(localPath));
        return;
    }

    beginTransfer(info.size());

    QString error;
    if (!uploadFile(localPath, remotePath, &error)) {
        endTransfer();
        if (m_transferCancel.load(std::memory_order_relaxed)) {
            emit sftpCanceled(error);
        } else {
            emit sftpError(error);
        }
        return;
    }

    endTransfer();
    emit sftpFinished(tr("Synced: %1").arg(QFileInfo(remotePath).fileName()));
}

void SshWorker::downloadPaths(const QStringList &remotePaths, const QString &localDir)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    QDir local(localDir);
    if (!local.exists() && !local.mkpath(QStringLiteral("."))) {
        emit sftpError(tr("Cannot create local directory: %1").arg(localDir));
        return;
    }

    beginTransfer(computeRemoteBytes(remotePaths));

    QString error;
    for (const QString &remotePath : remotePaths) {
        if (transferCanceled(&error)) {
            endTransfer();
            emit sftpCanceled(error);
            return;
        }

        bool isDir = false;
        if (!isRemoteDirectory(remotePath, &isDir, &error)) {
            endTransfer();
            emit sftpError(error);
            return;
        }

        const QString name = QFileInfo(remotePath).fileName();
        const QString localPath = local.filePath(name);
        if (!downloadPathRecursive(remotePath, localPath, isDir, &error)) {
            endTransfer();
            if (m_transferCancel.load(std::memory_order_relaxed)) {
                emit sftpCanceled(error);
            } else {
                emit sftpError(error);
            }
            return;
        }
    }

    endTransfer();
    emit sftpFinished(tr("Download finished (%1 item(s))").arg(remotePaths.size()));
}

void SshWorker::cancelTransfer()
{
    m_transferCancel.store(true, std::memory_order_relaxed);
}

void SshWorker::canonicalizePath(const QString &path)
{
    if (!m_running || m_sftp == nullptr) {
        emit sftpError(tr("SFTP is not available"));
        return;
    }

    const QString requested = path.isEmpty() ? QStringLiteral(".") : path;
    const QByteArray remote = requested.toUtf8();
    char *canonical = sftp_canonicalize_path(m_sftp, remote.constData());
    if (canonical == nullptr) {
        // Fall back to the requested path when canonicalize fails (e.g. missing dir).
        emit pathCanonicalized(requested, requested);
        return;
    }

    const QString result = QString::fromUtf8(canonical);
    ssh_string_free_char(canonical);
    emit pathCanonicalized(requested, result);
}

void SshWorker::pollChannel()
{
    if (!m_running || m_session == nullptr) {
        return;
    }

    acceptRemoteForwards();
    pollTunnelBridges();

    if (m_channel == nullptr) {
        return;
    }

    if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel)) {
        disconnectSession();
        return;
    }

    // Cap per-tick reads so a fullscreen htop redraw cannot flood the UI queue.
    constexpr int kMaxBytesPerTick = 64 * 1024;
    int totalRead = 0;
    char buffer[4096];

    while (totalRead < kMaxBytesPerTick) {
        const int nbytes = ssh_channel_read_nonblocking(m_channel, buffer, sizeof(buffer), 0);
        if (nbytes == SSH_EOF) {
            // Shell exited (e.g. user typed "exit") — clean disconnect, not an error.
            disconnectSession();
            return;
        }
        if (nbytes < 0) {
            if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel) ||
                (m_session && !ssh_is_connected(m_session))) {
                disconnectSession();
                return;
            }
            emit errorOccurred(tr("Read error: %1").arg(sessionError()));
            disconnectSession();
            return;
        }
        if (nbytes == 0) {
            break;
        }
        totalRead += nbytes;
        emit dataReceived(QByteArray(buffer, nbytes));
    }

    while (totalRead < kMaxBytesPerTick) {
        const int nbytes = ssh_channel_read_nonblocking(m_channel, buffer, sizeof(buffer), 1);
        if (nbytes == SSH_EOF) {
            disconnectSession();
            return;
        }
        if (nbytes <= 0) {
            break;
        }
        totalRead += nbytes;
        emit dataReceived(QByteArray(buffer, nbytes));
    }

    if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel)) {
        disconnectSession();
    }
}

bool SshWorker::verifyKnownHost()
{
    const enum ssh_known_hosts_e state = ssh_session_is_known_server(m_session);

    switch (state) {
    case SSH_KNOWN_HOSTS_OK:
        return true;

    case SSH_KNOWN_HOSTS_CHANGED:
        return promptHostKeyAndUpdate(HostKeyPrompt::Changed, true);

    case SSH_KNOWN_HOSTS_OTHER:
        return promptHostKeyAndUpdate(HostKeyPrompt::Other, true);

    case SSH_KNOWN_HOSTS_ERROR:
        emit errorOccurred(tr("Error checking known hosts: %1").arg(sessionError()));
        return false;

    case SSH_KNOWN_HOSTS_NOT_FOUND:
    case SSH_KNOWN_HOSTS_UNKNOWN:
        return promptHostKeyAndUpdate(HostKeyPrompt::Unknown, false);
    }

    emit errorOccurred(tr("Unexpected known_hosts state"));
    return false;
}

bool SshWorker::promptHostKeyAndUpdate(HostKeyPrompt reason, bool removeExistingEntries)
{
    const QString fingerprint = fingerprintOf(m_session);
    if (fingerprint.isEmpty()) {
        emit errorOccurred(tr("Unable to read server host key fingerprint"));
        return false;
    }

    qCWarning(lcSsh) << "Host key prompt" << static_cast<int>(reason) << fingerprint;

    {
        QMutexLocker locker(&m_hostKeyMutex);
        m_hostKeyAnswered = false;
        m_hostKeyAccepted = false;
    }

    emit hostKeyPrompt(reason, fingerprint);

    {
        QMutexLocker locker(&m_hostKeyMutex);
        while (!m_hostKeyAnswered) {
            m_hostKeyCondition.wait(&m_hostKeyMutex);
        }
        if (!m_hostKeyAccepted) {
            qCWarning(lcSsh) << "Host key rejected by user";
            emit errorOccurred(tr("Host key was rejected"));
            return false;
        }
    }

    if (removeExistingEntries && !removeKnownHostsEntriesForSession()) {
        emit errorOccurred(tr("Failed to update known_hosts: could not remove old host key"));
        return false;
    }

    if (ssh_session_update_known_hosts(m_session) != SSH_OK) {
        emit errorOccurred(tr("Failed to update known_hosts: %1").arg(sessionError()));
        return false;
    }

    qCWarning(lcSsh) << "Host key accepted and known_hosts updated";
    return true;
}

QString SshWorker::knownHostsFilePath() const
{
    if (m_session != nullptr) {
        char *path = nullptr;
        if (ssh_options_get(m_session, SSH_OPTIONS_KNOWNHOSTS, &path) == SSH_OK &&
            path != nullptr) {
            const QString result = QString::fromUtf8(path);
            ssh_string_free_char(path);
            if (!result.isEmpty()) {
                return result;
            }
        }
    }
    return QDir::home().filePath(QStringLiteral(".ssh/known_hosts"));
}

bool SshWorker::knownHostsLineMatchesHost(const QString &hostField, const QString &host, int port)
{
    if (hostField.isEmpty() || host.isEmpty()) {
        return false;
    }
    // Hashed known_hosts entries cannot be matched by hostname here.
    if (hostField.startsWith(QLatin1Char('|'))) {
        return false;
    }

    const QString bracketed = QStringLiteral("[%1]:%2").arg(host).arg(port);
    const QStringList names = hostField.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString name : names) {
        name = name.trimmed();
        if (name.compare(host, Qt::CaseInsensitive) == 0 ||
            name.compare(bracketed, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool SshWorker::removeKnownHostsEntriesForSession()
{
    if (m_session == nullptr) {
        return false;
    }

    char *hostC = nullptr;
    if (ssh_options_get(m_session, SSH_OPTIONS_HOST, &hostC) != SSH_OK || hostC == nullptr) {
        return false;
    }
    const QString host = QString::fromUtf8(hostC);
    ssh_string_free_char(hostC);

    unsigned int port = 22;
    ssh_options_get_port(m_session, &port);

    const QString path = knownHostsFilePath();
    QFile in(path);
    if (!in.exists()) {
        return true;
    }
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcSsh) << "Cannot read known_hosts:" << path << in.errorString();
        return false;
    }

    QStringList kept;
    kept.reserve(64);
    QTextStream stream(&in);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            kept.append(line);
            continue;
        }

        const int space = trimmed.indexOf(QLatin1Char(' '));
        const QString hostField = space > 0 ? trimmed.left(space) : trimmed;
        if (knownHostsLineMatchesHost(hostField, host, static_cast<int>(port))) {
            continue;
        }
        kept.append(line);
    }
    in.close();

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(lcSsh) << "Cannot write known_hosts:" << path << out.errorString();
        return false;
    }
    QTextStream outStream(&out);
    for (const QString &line : kept) {
        outStream << line << QLatin1Char('\n');
    }
    if (!out.commit()) {
        qCWarning(lcSsh) << "Failed to commit known_hosts:" << path;
        return false;
    }
    return true;
}

bool SshWorker::authenticate(const Connection &connection, QString &secret)
{
    int rc = ssh_userauth_none(m_session, nullptr);
    if (rc == SSH_AUTH_ERROR) {
        emit errorOccurred(tr("Authentication error: %1").arg(sessionError()));
        return false;
    }
    if (rc == SSH_AUTH_SUCCESS) {
        return true;
    }

    switch (connection.authType) {
    case AuthType::PrivateKey: {
        // Agent-first: try SSH agent before any key file / default identities.
        if (authenticateWithAgent()) {
            return true;
        }

        if (!connection.privateKeyPath.isEmpty()) {
            if (authenticatePrivateKey(connection.privateKeyPath, secret)) {
                return true;
            }
            return false;
        }

        if (authenticatePublicKeyAuto(secret)) {
            return true;
        }

        emit errorOccurred(tr("Authentication failed: no usable agent identity or default key"));
        return false;
    }
    case AuthType::Password:
    default: {
        if (secret.isEmpty()) {
            emit errorOccurred(tr("Password is empty"));
            return false;
        }

        if (authenticatePassword(secret)) {
            return true;
        }

        // Many servers disable password auth and only offer keyboard-interactive.
        const int methods = ssh_userauth_list(m_session, nullptr);
        if (methods & SSH_AUTH_METHOD_INTERACTIVE) {
            if (authenticateKeyboardInteractive(secret)) {
                return true;
            }
        }

        emit errorOccurred(tr("Authentication failed: %1").arg(sessionError()));
        return false;
    }
    }
}

bool SshWorker::authenticatePassword(const QString &password)
{
    const QByteArray pwd = password.toUtf8();
    const int rc = ssh_userauth_password(m_session, nullptr, pwd.constData());
    return rc == SSH_AUTH_SUCCESS;
}

bool SshWorker::authenticateKeyboardInteractive(const QString &password)
{
    int rc = ssh_userauth_kbdint(m_session, nullptr, nullptr);
    while (rc == SSH_AUTH_INFO) {
        const int nprompts = ssh_userauth_kbdint_getnprompts(m_session);
        for (int i = 0; i < nprompts; ++i) {
            char echo = 0;
            const char *prompt = ssh_userauth_kbdint_getprompt(m_session, i, &echo);
            Q_UNUSED(prompt);
            Q_UNUSED(echo);
            const QByteArray answer = password.toUtf8();
            if (ssh_userauth_kbdint_setanswer(m_session, i, answer.constData()) < 0) {
                return false;
            }
        }
        rc = ssh_userauth_kbdint(m_session, nullptr, nullptr);
    }
    return rc == SSH_AUTH_SUCCESS;
}

bool SshWorker::authenticateWithAgent()
{
    const int methods = ssh_userauth_list(m_session, nullptr);
    if (!(methods & SSH_AUTH_METHOD_PUBLICKEY)) {
        return false;
    }

    const int rc = ssh_userauth_agent(m_session, nullptr);
    return rc == SSH_AUTH_SUCCESS;
}

bool SshWorker::authenticatePrivateKey(const QString &keyPath, const QString &passphrase)
{
    if (keyPath.isEmpty()) {
        return false;
    }

    ssh_key key = nullptr;
    const QByteArray path = keyPath.toUtf8();
    const QByteArray phrase = passphrase.toUtf8();
    const char *phrasePtr = passphrase.isEmpty() ? nullptr : phrase.constData();

    int rc = ssh_pki_import_privkey_file(path.constData(), phrasePtr, nullptr, nullptr, &key);
    if (rc != SSH_OK) {
        emit errorOccurred(tr("Failed to load private key: %1").arg(sessionError()));
        return false;
    }

    rc = ssh_userauth_publickey(m_session, nullptr, key);
    ssh_key_free(key);

    if (rc != SSH_AUTH_SUCCESS) {
        emit errorOccurred(tr("Private key authentication failed: %1").arg(sessionError()));
        return false;
    }
    return true;
}

bool SshWorker::authenticatePublicKeyAuto(const QString &passphrase)
{
    const QByteArray phrase = passphrase.toUtf8();
    const char *phrasePtr = passphrase.isEmpty() ? nullptr : phrase.constData();
    const int rc = ssh_userauth_publickey_auto(m_session, nullptr, phrasePtr);
    return rc == SSH_AUTH_SUCCESS;
}

bool SshWorker::openShell()
{
    m_channel = ssh_channel_new(m_session);
    if (m_channel == nullptr) {
        emit errorOccurred(tr("Failed to create channel: %1").arg(sessionError()));
        return false;
    }

    if (ssh_channel_open_session(m_channel) != SSH_OK) {
        emit errorOccurred(tr("Failed to open channel: %1").arg(sessionError()));
        return false;
    }

    // Request PTY with real size + TERM that full-screen apps (vim) understand.
    // ssh_channel_request_pty() alone defaults to 80x24 and a generic term type.
    if (ssh_channel_request_pty_size(m_channel, "xterm-256color", m_ptyCols, m_ptyRows) != SSH_OK) {
        emit errorOccurred(tr("Failed to request PTY: %1").arg(sessionError()));
        return false;
    }

    if (ssh_channel_request_shell(m_channel) != SSH_OK) {
        emit errorOccurred(tr("Failed to request shell: %1").arg(sessionError()));
        return false;
    }

    return true;
}

bool SshWorker::openSftp(QString *failureMessage)
{
    m_sftp = sftp_new(m_session);
    if (m_sftp == nullptr) {
        if (failureMessage) {
            *failureMessage = tr("Failed to create SFTP session: %1").arg(sessionError());
        }
        return false;
    }

    if (sftp_init(m_sftp) != SSH_OK) {
        if (failureMessage) {
            const QString detail =
                sftpErrorMessage().isEmpty() ? sessionError() : sftpErrorMessage();
            if (detail.contains(QStringLiteral("subsystem"), Qt::CaseInsensitive) ||
                sessionError().contains(QStringLiteral("subsystem"), Qt::CaseInsensitive)) {
                *failureMessage =
                    tr("This server does not support SFTP (subsystem request failed).");
            } else {
                *failureMessage = tr("SFTP is unavailable: %1").arg(detail);
            }
        }
        sftp_free(m_sftp);
        m_sftp = nullptr;
        return false;
    }

    return true;
}

void SshWorker::cleanup()
{
    m_running = false;

    if (m_ioTimer) {
        m_ioTimer->stop();
    }

    stopAllTunnels();

    if (m_sftp) {
        sftp_free(m_sftp);
        m_sftp = nullptr;
    }

    if (m_channel) {
        if (ssh_channel_is_open(m_channel)) {
            ssh_channel_send_eof(m_channel);
            ssh_channel_close(m_channel);
        }
        ssh_channel_free(m_channel);
        m_channel = nullptr;
    }

    if (m_session) {
        if (ssh_is_connected(m_session)) {
            ssh_disconnect(m_session);
        }
        ssh_free(m_session);
        m_session = nullptr;
    }
}

QString SshWorker::sessionError() const
{
    if (m_session == nullptr) {
        return tr("Unknown error");
    }
    const char *err = ssh_get_error(m_session);
    return err ? QString::fromUtf8(err) : tr("Unknown error");
}

QString SshWorker::sftpErrorMessage() const
{
    if (m_sftp == nullptr) {
        return sessionError();
    }

    const int code = sftp_get_error(m_sftp);
    switch (code) {
    case SSH_FX_OK:
        return sessionError();
    case SSH_FX_NO_SUCH_FILE:
        return tr("No such file or directory");
    case SSH_FX_PERMISSION_DENIED:
        return tr("Permission denied");
    case SSH_FX_FAILURE:
        return tr("SFTP failure");
    case SSH_FX_BAD_MESSAGE:
        return tr("Bad SFTP message");
    case SSH_FX_NO_CONNECTION:
        return tr("No SFTP connection");
    case SSH_FX_CONNECTION_LOST:
        return tr("SFTP connection lost");
    case SSH_FX_OP_UNSUPPORTED:
        return tr("SFTP operation unsupported");
    case SSH_FX_INVALID_HANDLE:
        return tr("Invalid SFTP handle");
    case SSH_FX_NO_SUCH_PATH:
        return tr("No such path");
    case SSH_FX_FILE_ALREADY_EXISTS:
        return tr("File already exists");
    case SSH_FX_WRITE_PROTECT:
        return tr("Write-protected filesystem");
#ifdef SSH_FX_NO_SPACE_ON_FILESYSTEM
    case SSH_FX_NO_SPACE_ON_FILESYSTEM:
        return tr("Disk full");
#endif
    default:
        return sessionError().isEmpty() ? tr("SFTP error %1").arg(code) : sessionError();
    }
}

QString SshWorker::localIoErrorMessage(const QString &qtErrorString)
{
    if (errno == ENOSPC ||
        qtErrorString.contains(QStringLiteral("No space"), Qt::CaseInsensitive) ||
        qtErrorString.contains(QStringLiteral("disk full"), Qt::CaseInsensitive)) {
        return tr("Disk full");
    }
    return qtErrorString;
}

void SshWorker::beginTransfer(qint64 bytesTotal)
{
    m_transferCancel.store(false, std::memory_order_relaxed);
    m_progressBytesDone = 0;
    m_progressBytesTotal = bytesTotal;
    m_progressLastEmitBytes = 0;
    m_progressLastEmitMs = QDateTime::currentMSecsSinceEpoch();
    emit sftpProgress(0, m_progressBytesTotal, QString());
}

void SshWorker::endTransfer()
{
    m_transferCancel.store(false, std::memory_order_relaxed);
}

bool SshWorker::transferCanceled(QString *error) const
{
    if (!m_transferCancel.load(std::memory_order_relaxed)) {
        return false;
    }
    if (error) {
        *error = tr("Transfer canceled");
    }
    return true;
}

void SshWorker::noteTransferProgress(qint64 bytesDelta, const QString &currentName)
{
    if (bytesDelta > 0) {
        m_progressBytesDone += bytesDelta;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool bytesThreshold =
        (m_progressBytesDone - m_progressLastEmitBytes) >= kProgressEmitBytes;
    const bool timeThreshold = (now - m_progressLastEmitMs) >= kProgressEmitMs;
    if (!bytesThreshold && !timeThreshold && bytesDelta > 0) {
        // Still pump the worker event loop so shell I/O / cancel stay responsive.
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        return;
    }

    m_progressLastEmitBytes = m_progressBytesDone;
    m_progressLastEmitMs = now;
    emit sftpProgress(m_progressBytesDone, m_progressBytesTotal, currentName);
    // Keep PTY polling and queued cancel slots alive during long transfers.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

qint64 SshWorker::computeLocalBytes(const QStringList &localPaths) const
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

qint64 SshWorker::computeLocalPathBytes(const QString &localPath) const
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

qint64 SshWorker::computeRemoteBytes(const QStringList &remotePaths)
{
    qint64 total = 0;
    for (const QString &path : remotePaths) {
        bool isDir = false;
        QString error;
        if (!isRemoteDirectory(path, &isDir, &error)) {
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

qint64 SshWorker::computeRemotePathBytes(const QString &remotePath, bool isDir)
{
    if (!isDir) {
        const QByteArray remote = remotePath.toUtf8();
        sftp_attributes attrs = sftp_stat(m_sftp, remote.constData());
        if (attrs == nullptr) {
            return -1;
        }
        const qint64 size = static_cast<qint64>(attrs->size);
        sftp_attributes_free(attrs);
        return size;
    }

    QVector<RemoteEntry> children;
    QString error;
    if (!listDirectoryEntries(remotePath, &children, &error)) {
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

QString SshWorker::fingerprintOf(ssh_session session)
{
    ssh_key srvPubkey = nullptr;
    if (ssh_get_server_publickey(session, &srvPubkey) < 0) {
        return {};
    }

    unsigned char *hash = nullptr;
    size_t hlen = 0;
    const int rc = ssh_get_publickey_hash(srvPubkey, SSH_PUBLICKEY_HASH_SHA256, &hash, &hlen);
    ssh_key_free(srvPubkey);
    if (rc < 0 || hash == nullptr) {
        return {};
    }

    // Prefer libssh's printable form when available.
    char *hexa = ssh_get_hexa(hash, hlen);
    QString result;
    if (hexa) {
        result = QString::fromUtf8(hexa);
        ssh_string_free_char(hexa);
    } else {
        result = QString::fromLatin1(
            QByteArray(reinterpret_cast<const char *>(hash), static_cast<int>(hlen)).toBase64());
    }
    ssh_clean_pubkey_hash(&hash);
    return result;
}

QString SshWorker::formatPermissions(uint32_t permissions, uint8_t type)
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

bool SshWorker::listDirectoryEntries(const QString &path,
                                     QVector<RemoteEntry> *outEntries,
                                     QString *error)
{
    const QByteArray remote = path.toUtf8();
    sftp_dir dir = sftp_opendir(m_sftp, remote.constData());
    if (dir == nullptr) {
        if (error) {
            *error = tr("Cannot open directory: %1").arg(sftpErrorMessage());
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
            *error = tr("Cannot list directory: %1").arg(sftpErrorMessage());
        }
        sftp_closedir(dir);
        return false;
    }

    if (sftp_closedir(dir) != SSH_OK) {
        if (error) {
            *error = tr("Cannot close directory: %1").arg(sftpErrorMessage());
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

bool SshWorker::removePathRecursive(const QString &path, QString *error)
{
    bool isDir = false;
    if (!isRemoteDirectory(path, &isDir, error)) {
        return false;
    }

    if (isDir) {
        QVector<RemoteEntry> children;
        if (!listDirectoryEntries(path, &children, error)) {
            return false;
        }
        for (const RemoteEntry &child : children) {
            if (!removePathRecursive(child.path, error)) {
                return false;
            }
        }

        const QByteArray remote = path.toUtf8();
        if (sftp_rmdir(m_sftp, remote.constData()) != SSH_OK) {
            if (error) {
                *error = tr("Cannot delete folder: %1").arg(sftpErrorMessage());
            }
            return false;
        }
        return true;
    }

    const QByteArray remote = path.toUtf8();
    if (sftp_unlink(m_sftp, remote.constData()) != SSH_OK) {
        if (error) {
            *error = tr("Cannot delete file: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SshWorker::uploadPathRecursive(const QString &localPath,
                                    const QString &remotePath,
                                    QString *error)
{
    if (transferCanceled(error)) {
        return false;
    }

    const QFileInfo info(localPath);
    if (info.isDir()) {
        const QByteArray remote = remotePath.toUtf8();
        if (sftp_mkdir(m_sftp, remote.constData(), S_IRWXU) != SSH_OK) {
            const int code = sftp_get_error(m_sftp);
            if (code != SSH_FX_FILE_ALREADY_EXISTS) {
                if (error) {
                    *error = tr("Cannot create remote folder: %1").arg(sftpErrorMessage());
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

    return uploadFile(localPath, remotePath, error);
}

bool SshWorker::downloadPathRecursive(const QString &remotePath,
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
                *error = (errno == ENOSPC) ? tr("Disk full")
                                           : tr("Cannot create local folder: %1").arg(localPath);
            }
            return false;
        }

        QVector<RemoteEntry> children;
        if (!listDirectoryEntries(remotePath, &children, error)) {
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

    return downloadFile(remotePath, localPath, error);
}

bool SshWorker::uploadFile(const QString &localPath, const QString &remotePath, QString *error)
{
    if (transferCanceled(error)) {
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = tr("Cannot open local file: %1").arg(localIoErrorMessage(local.errorString()));
        }
        return false;
    }

    const QByteArray remote = remotePath.toUtf8();
    const int access = O_WRONLY | O_CREAT | O_TRUNC;
    sftp_file file =
        sftp_open(m_sftp, remote.constData(), access, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file == nullptr) {
        if (error) {
            *error = tr("Cannot open remote file for writing: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    const QString displayName = QFileInfo(localPath).fileName();
    noteTransferProgress(0, displayName);

    char buffer[kXferBufSize];
    while (!local.atEnd()) {
        if (transferCanceled(error)) {
            sftp_close(file);
            return false;
        }

        const qint64 nread = local.read(buffer, static_cast<qint64>(sizeof(buffer)));
        if (nread < 0) {
            if (error) {
                *error =
                    tr("Cannot read local file: %1").arg(localIoErrorMessage(local.errorString()));
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
            if (transferCanceled(error)) {
                sftp_close(file);
                return false;
            }

            const ssize_t nwritten = sftp_write(file, ptr, static_cast<size_t>(remaining));
            if (nwritten < 0) {
                if (error) {
                    *error = tr("Cannot write remote file: %1").arg(sftpErrorMessage());
                }
                sftp_close(file);
                return false;
            }
            ptr += nwritten;
            remaining -= nwritten;
            noteTransferProgress(nwritten, displayName);
        }
    }

    if (sftp_close(file) != SSH_OK) {
        if (error) {
            *error = tr("Cannot close remote file: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SshWorker::downloadFile(const QString &remotePath, const QString &localPath, QString *error)
{
    if (transferCanceled(error)) {
        return false;
    }

    const QByteArray remote = remotePath.toUtf8();
    sftp_file file = sftp_open(m_sftp, remote.constData(), O_RDONLY, 0);
    if (file == nullptr) {
        if (error) {
            *error = tr("Cannot open remote file for reading: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = tr("Cannot open local file for writing: %1")
                         .arg(localIoErrorMessage(local.errorString()));
        }
        sftp_close(file);
        return false;
    }

    const QString displayName = QFileInfo(remotePath).fileName();
    noteTransferProgress(0, displayName);

    char buffer[kXferBufSize];
    for (;;) {
        if (transferCanceled(error)) {
            sftp_close(file);
            return false;
        }

        const ssize_t nbytes = sftp_read(file, buffer, sizeof(buffer));
        if (nbytes == 0) {
            break;
        }
        if (nbytes < 0) {
            if (error) {
                *error = tr("Cannot read remote file: %1").arg(sftpErrorMessage());
            }
            sftp_close(file);
            return false;
        }

        if (local.write(buffer, nbytes) != nbytes) {
            if (error) {
                *error =
                    tr("Cannot write local file: %1").arg(localIoErrorMessage(local.errorString()));
            }
            sftp_close(file);
            return false;
        }
        noteTransferProgress(nbytes, displayName);
    }

    if (sftp_close(file) != SSH_OK) {
        if (error) {
            *error = tr("Cannot close remote file: %1").arg(sftpErrorMessage());
        }
        return false;
    }
    return true;
}

bool SshWorker::isRemoteDirectory(const QString &path, bool *isDir, QString *error)
{
    const QByteArray remote = path.toUtf8();
    sftp_attributes attributes = sftp_stat(m_sftp, remote.constData());
    if (attributes == nullptr) {
        if (error) {
            *error = tr("Cannot stat path: %1").arg(sftpErrorMessage());
        }
        return false;
    }

    if (isDir) {
        *isDir = attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
    }
    sftp_attributes_free(attributes);
    return true;
}

void SshWorker::startTunnel(const TunnelDefinition &def)
{
    if (!m_running || m_session == nullptr) {
        emit tunnelError(def.id, tr("SSH session is not connected"));
        emit tunnelStatusChanged(
            def.id, QStringLiteral("Error"), tr("SSH session is not connected"));
        return;
    }

    if (def.id.isNull() || def.localPort == 0 || def.remotePort == 0) {
        emit tunnelError(def.id, tr("Invalid tunnel definition"));
        emit tunnelStatusChanged(def.id, QStringLiteral("Error"), tr("Invalid tunnel definition"));
        return;
    }

    if (m_tunnels.contains(def.id)) {
        stopTunnel(def.id);
    }

    emit tunnelStatusChanged(def.id, QStringLiteral("Starting"), QString());

    auto *tunnel = new ActiveTunnel;
    tunnel->def = def;
    m_tunnels.insert(def.id, tunnel);

    bool ok = false;
    if (def.type == TunnelType::Remote) {
        ok = startRemoteTunnel(tunnel);
    } else {
        ok = startLocalTunnel(tunnel);
    }

    if (!ok) {
        m_tunnels.remove(def.id);
        destroyTunnel(tunnel, false);
        return;
    }

    emit tunnelStatusChanged(def.id, QStringLiteral("Listening"), QString());
}

void SshWorker::stopTunnel(const QUuid &tunnelId)
{
    ActiveTunnel *tunnel = m_tunnels.take(tunnelId);
    if (tunnel == nullptr) {
        emit tunnelStatusChanged(tunnelId, QStringLiteral("Off"), QString());
        return;
    }

    destroyTunnel(tunnel, true);
}

void SshWorker::stopAllTunnels()
{
    const QList<QUuid> ids = m_tunnels.keys();
    for (const QUuid &id : ids) {
        ActiveTunnel *tunnel = m_tunnels.take(id);
        if (tunnel) {
            destroyTunnel(tunnel, true);
        }
    }
}

bool SshWorker::startLocalTunnel(ActiveTunnel *tunnel)
{
    auto *server = new QTcpServer(this);
    QHostAddress address;
    if (tunnel->def.localHost.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 ||
        tunnel->def.localHost == QLatin1String("127.0.0.1")) {
        address = QHostAddress::LocalHost;
    } else if (tunnel->def.localHost.compare(QLatin1String("::1"), Qt::CaseInsensitive) == 0) {
        address = QHostAddress::LocalHostIPv6;
    } else {
        address = QHostAddress(tunnel->def.localHost);
    }
    if (address.isNull()) {
        const QString message = tr("Invalid local bind address: %1").arg(tunnel->def.localHost);
        emit tunnelError(tunnel->def.id, message);
        emit tunnelStatusChanged(tunnel->def.id, QStringLiteral("Error"), message);
        server->deleteLater();
        return false;
    }

    if (!server->listen(address, tunnel->def.localPort)) {
        const QString message =
            tr("Cannot listen on %1: %2").arg(tunnel->def.localAddress(), server->errorString());
        emit tunnelError(tunnel->def.id, message);
        emit tunnelStatusChanged(tunnel->def.id, QStringLiteral("Error"), message);
        server->deleteLater();
        return false;
    }

    tunnel->server = server;
    connect(server, &QTcpServer::newConnection, this, &SshWorker::onLocalTunnelNewConnection);
    return true;
}

bool SshWorker::startRemoteTunnel(ActiveTunnel *tunnel)
{
    const QByteArray address = tunnel->def.remoteHost.toUtf8();
    int boundPort = 0;
    const int rc = ssh_channel_listen_forward(m_session,
                                              address.isEmpty() ? nullptr : address.constData(),
                                              tunnel->def.remotePort,
                                              &boundPort);

    if (rc != SSH_OK) {
        const QString message = tr("Remote listen failed: %1").arg(sessionError());
        emit tunnelError(tunnel->def.id, message);
        emit tunnelStatusChanged(tunnel->def.id, QStringLiteral("Error"), message);
        return false;
    }

    tunnel->remoteListening = true;
    if (boundPort > 0) {
        tunnel->def.remotePort = static_cast<quint16>(boundPort);
    }
    return true;
}

void SshWorker::onLocalTunnelNewConnection()
{
    auto *server = qobject_cast<QTcpServer *>(sender());
    if (server == nullptr) {
        return;
    }

    ActiveTunnel *tunnel = tunnelForServer(server);
    if (tunnel == nullptr) {
        while (server->hasPendingConnections()) {
            QTcpSocket *socket = server->nextPendingConnection();
            if (socket) {
                socket->abort();
                socket->deleteLater();
            }
        }
        return;
    }

    while (server->hasPendingConnections()) {
        QTcpSocket *socket = server->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        if (!openLocalForwardBridge(tunnel, socket)) {
            socket->abort();
            socket->deleteLater();
        }
    }
}

bool SshWorker::openLocalForwardBridge(ActiveTunnel *tunnel, QTcpSocket *socket)
{
    ssh_channel channel = ssh_channel_new(m_session);
    if (channel == nullptr) {
        emit tunnelError(tunnel->def.id, tr("Failed to allocate forward channel"));
        return false;
    }

    const QByteArray remoteHost = tunnel->def.remoteHost.toUtf8();
    const QByteArray sourceHost = socket->peerAddress().toString().toUtf8();
    const int sourcePort = static_cast<int>(socket->peerPort());

    // open_forward must run in blocking mode. ssh_channel_set_blocking(0) flips the
    // whole session non-blocking and later opens return SSH_AGAIN (treated as failure),
    // which broke Disable → Enable and multi-connection use.
    ssh_set_blocking(m_session, 1);

    const int rc =
        ssh_channel_open_forward(channel,
                                 remoteHost.constData(),
                                 tunnel->def.remotePort,
                                 sourceHost.isEmpty() ? "127.0.0.1" : sourceHost.constData(),
                                 sourcePort > 0 ? sourcePort : 0);

    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        const QString message = tr("Forward open failed: %1").arg(sessionError());
        emit tunnelError(tunnel->def.id, message);
        return false;
    }

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = tunnel->def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    tunnel->bridges.append(bridge);

    socket->setParent(this);
    connect(socket, &QTcpSocket::readyRead, this, &SshWorker::onBridgeSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &SshWorker::onBridgeSocketDisconnected);
    return true;
}

void SshWorker::acceptRemoteForwards()
{
    if (m_session == nullptr) {
        return;
    }

    bool hasRemote = false;
    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel && tunnel->remoteListening) {
            hasRemote = true;
            break;
        }
    }
    if (!hasRemote) {
        return;
    }

    // Accept any pending reverse-forward channels (timeout 0 = non-blocking).
    for (int i = 0; i < 8; ++i) {
        int destinationPort = 0;
        char *originator = nullptr;
        int originatorPort = 0;
        ssh_channel channel = ssh_channel_open_forward_port(
            m_session, 0, &destinationPort, &originator, &originatorPort);
        if (originator) {
            ssh_string_free_char(originator);
            originator = nullptr;
        }
        if (channel == nullptr) {
            break;
        }

        ActiveTunnel *match = nullptr;
        for (ActiveTunnel *tunnel : m_tunnels) {
            if (tunnel && tunnel->remoteListening &&
                tunnel->def.remotePort == static_cast<quint16>(destinationPort)) {
                match = tunnel;
                break;
            }
        }

        // Some servers report destination_port as 0; fall back to single remote tunnel.
        if (match == nullptr) {
            for (ActiveTunnel *tunnel : m_tunnels) {
                if (tunnel && tunnel->remoteListening) {
                    match = tunnel;
                    break;
                }
            }
        }

        if (match == nullptr) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            continue;
        }

        if (!openRemoteForwardBridge(match, channel)) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
        }
    }
}

bool SshWorker::openRemoteForwardBridge(ActiveTunnel *tunnel, ssh_channel channel)
{
    auto *socket = new QTcpSocket(this);
    socket->connectToHost(tunnel->def.localHost, tunnel->def.localPort);
    if (!socket->waitForConnected(5000)) {
        const QString message = tr("Cannot connect to local %1: %2")
                                    .arg(tunnel->def.localAddress(), socket->errorString());
        emit tunnelError(tunnel->def.id, message);
        socket->deleteLater();
        return false;
    }

    auto *bridge = new TunnelBridge;
    bridge->tunnelId = tunnel->def.id;
    bridge->channel = channel;
    bridge->socket = socket;
    tunnel->bridges.append(bridge);

    connect(socket, &QTcpSocket::readyRead, this, &SshWorker::onBridgeSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &SshWorker::onBridgeSocketDisconnected);
    return true;
}

void SshWorker::onBridgeSocketReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr) {
        return;
    }

    TunnelBridge *bridge = bridgeForSocket(socket);
    if (bridge == nullptr || bridge->closing || bridge->channel == nullptr) {
        return;
    }

    const QByteArray data = socket->readAll();
    if (data.isEmpty()) {
        return;
    }

    const char *ptr = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        const int written =
            ssh_channel_write(bridge->channel, ptr, static_cast<uint32_t>(remaining));
        if (written == SSH_AGAIN) {
            // Should be rare while session is blocking; avoid dropping the bridge.
            break;
        }
        if (written == SSH_ERROR || written < 0) {
            closeBridge(bridge);
            return;
        }
        if (written == 0) {
            break;
        }
        ptr += written;
        remaining -= written;
    }
}

void SshWorker::onBridgeSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr) {
        return;
    }

    TunnelBridge *bridge = bridgeForSocket(socket);
    if (bridge) {
        closeBridge(bridge);
    }
}

void SshWorker::pollTunnelBridges()
{
    char buffer[8192];
    QList<TunnelBridge *> toClose;

    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel == nullptr) {
            continue;
        }
        for (TunnelBridge *bridge : tunnel->bridges) {
            if (bridge == nullptr || bridge->closing || bridge->channel == nullptr ||
                bridge->socket == nullptr) {
                continue;
            }

            if (!ssh_channel_is_open(bridge->channel) || ssh_channel_is_eof(bridge->channel)) {
                toClose.append(bridge);
                continue;
            }

            while (true) {
                const int nbytes =
                    ssh_channel_read_nonblocking(bridge->channel, buffer, sizeof(buffer), 0);
                if (nbytes == SSH_EOF || nbytes < 0) {
                    toClose.append(bridge);
                    break;
                }
                if (nbytes == 0) {
                    break;
                }
                const qint64 written = bridge->socket->write(buffer, nbytes);
                if (written < 0) {
                    toClose.append(bridge);
                    break;
                }
            }
        }
    }

    for (TunnelBridge *bridge : toClose) {
        closeBridge(bridge);
    }
}

void SshWorker::closeBridge(TunnelBridge *bridge)
{
    if (bridge == nullptr || bridge->closing) {
        return;
    }
    bridge->closing = true;

    ActiveTunnel *owner = m_tunnels.value(bridge->tunnelId, nullptr);
    if (owner) {
        owner->bridges.removeAll(bridge);
    }

    if (bridge->socket) {
        disconnect(bridge->socket, nullptr, this, nullptr);
        bridge->socket->abort();
        bridge->socket->deleteLater();
        bridge->socket = nullptr;
    }

    if (bridge->channel) {
        if (ssh_channel_is_open(bridge->channel)) {
            ssh_channel_send_eof(bridge->channel);
            ssh_channel_close(bridge->channel);
        }
        ssh_channel_free(bridge->channel);
        bridge->channel = nullptr;
    }

    delete bridge;
}

void SshWorker::destroyTunnel(ActiveTunnel *tunnel, bool emitOff)
{
    if (tunnel == nullptr) {
        return;
    }

    const QUuid id = tunnel->def.id;

    const QList<TunnelBridge *> bridges = tunnel->bridges;
    for (TunnelBridge *bridge : bridges) {
        closeBridge(bridge);
    }
    tunnel->bridges.clear();

    if (tunnel->server) {
        disconnect(tunnel->server, nullptr, this, nullptr);
        tunnel->server->close();
        // Free synchronously so the local port can be rebound on Enable.
        delete tunnel->server;
        tunnel->server = nullptr;
    }

    if (tunnel->remoteListening && m_session) {
        const QByteArray address = tunnel->def.remoteHost.toUtf8();
        ssh_channel_cancel_forward(
            m_session, address.isEmpty() ? nullptr : address.constData(), tunnel->def.remotePort);
        tunnel->remoteListening = false;
    }

    delete tunnel;

    if (emitOff) {
        emit tunnelStatusChanged(id, QStringLiteral("Off"), QString());
    }
}

SshWorker::ActiveTunnel *SshWorker::tunnelForServer(QTcpServer *server)
{
    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel && tunnel->server == server) {
            return tunnel;
        }
    }
    return nullptr;
}

SshWorker::TunnelBridge *SshWorker::bridgeForSocket(QTcpSocket *socket)
{
    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel == nullptr) {
            continue;
        }
        for (TunnelBridge *bridge : tunnel->bridges) {
            if (bridge && bridge->socket == socket) {
                return bridge;
            }
        }
    }
    return nullptr;
}

SshWorker::TunnelBridge *SshWorker::bridgeForChannel(ssh_channel channel)
{
    for (ActiveTunnel *tunnel : m_tunnels) {
        if (tunnel == nullptr) {
            continue;
        }
        for (TunnelBridge *bridge : tunnel->bridges) {
            if (bridge && bridge->channel == channel) {
                return bridge;
            }
        }
    }
    return nullptr;
}
