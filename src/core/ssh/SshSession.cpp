#include "SshSession.h"

#include "SshAuth.h"
#include "core/util/Logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QtGlobal>

#include <libssh/server.h>

namespace
{
constexpr auto kClientInitBufferError = "Failed to construct client init buffer";

QString trSession(const char *text)
{
    return QCoreApplication::translate("SshSession", text);
}

QString optionString(ssh_session session, enum ssh_options_e type)
{
    char *value = nullptr;
    if (ssh_options_get(session, type, &value) != SSH_OK || value == nullptr) {
        return {};
    }
    const QString result = QString::fromUtf8(value);
    ssh_string_free_char(value);
    return result;
}
} // namespace

SshSession::~SshSession()
{
    cleanup();
}

void SshSession::setHostKeyVerifier(HostKeyVerifyFn verifier)
{
    m_hostKeyVerifier = std::move(verifier);
}

bool SshSession::isConnected() const
{
    return m_session != nullptr && ssh_is_connected(m_session);
}

QString SshSession::sessionError() const
{
    return sessionErrorOf(m_session);
}

QString SshSession::sessionErrorOf(ssh_session session)
{
    if (session == nullptr) {
        return trSession("Unknown error");
    }
    const char *err = ssh_get_error(session);
    return err ? QString::fromUtf8(err) : trSession("Unknown error");
}

void SshSession::configureKeepAlive(int intervalSec, int countMax)
{
    m_keepAliveIntervalSec = qMax(0, intervalSec);
    m_keepAliveCountMax = qMax(1, countMax);
    m_lastKeepAliveMs = 0;
    m_keepAliveMissed = 0;
}

void SshSession::resetKeepAliveClock()
{
    m_lastKeepAliveMs = QDateTime::currentMSecsSinceEpoch();
    m_keepAliveMissed = 0;
}

void SshSession::cleanup()
{
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

    m_jumpContexts.clear();
    m_jumpCallbacks.clear();
}

bool SshSession::establish(const Connection &connection,
                           const SessionCredentials &credentials,
                           int cols,
                           int rows,
                           QString *errorOut)
{
    cleanup();

    m_connection = connection;
    m_credentials = credentials;
    configureKeepAlive(connection.keepAliveIntervalSec, connection.keepAliveCountMax);

    qCWarning(lcSsh) << "Connecting to" << connection.username + QLatin1Char('@') + connection.host
                     << "port" << connection.port
                     << (connection.usesJumpHost() ? "(via gateway)" : "(direct)");

    m_session = ssh_new();
    if (m_session == nullptr) {
        if (errorOut) {
            *errorOut = trSession("Failed to create SSH session");
        }
        return false;
    }

    if (!applyConnectionOptions(connection)) {
        if (errorOut) {
            *errorOut = sessionError();
        }
        cleanup();
        return false;
    }

    QString connectError;
    if (!connectWithFallback(connection, &connectError)) {
        QString err = connectError.isEmpty() ? sessionError() : connectError;
        if (connection.usesJumpHost()) {
            err = trSession("Gateway: %1").arg(err);
        }
        qCWarning(lcSsh) << "Connection failed:" << err;
        if (errorOut) {
            *errorOut = err;
        }
        cleanup();
        return false;
    }

    if (!m_hostKeyVerifier || !m_hostKeyVerifier(m_session, QString())) {
        cleanup();
        return false;
    }

    QString mutableSecret = credentials.targetSecret;
    if (!SshAuth::authenticateSession(m_session, connection, mutableSecret)) {
        qCWarning(lcSsh) << "Authentication failed for" << connection.host;
        mutableSecret.fill(QChar(u'\0'));
        if (errorOut) {
            *errorOut = trSession("Authentication failed: %1").arg(sessionError());
        }
        cleanup();
        return false;
    }
    mutableSecret.fill(QChar(u'\0'));

    if (!openShell(cols, rows, errorOut)) {
        cleanup();
        return false;
    }

    resetKeepAliveClock();
    return true;
}


bool SshSession::applyConnectionOptions(const Connection &connection)
{
    if (m_session == nullptr) {
        return false;
    }

    if (connection.source == ConnectionSource::SshConfig && !connection.configAlias.isEmpty()) {
        // Match OpenSSH: set Host to the config alias, then let libssh apply ~/.ssh/config
        // (including Include / HostName / User / Port / IdentityFile / ProxyJump).
        const QByteArray alias = connection.configAlias.toUtf8();
        ssh_options_set(m_session, SSH_OPTIONS_HOST, alias.constData());
        if (ssh_options_parse_config(m_session, nullptr) != SSH_OK) {
            qCWarning(lcSsh) << "ssh_options_parse_config failed for alias"
                             << connection.configAlias << ":" << sessionErrorOf(m_session);
        }
        applyAdvancedOptions(connection);
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

        if (connection.usesJumpHost()) {
            const QByteArray proxyJump = connection.proxyJumpString().toUtf8();
            if (proxyJump.isEmpty()) {
                qCWarning(lcSsh) << "ProxyJump string is empty despite configured hops";
                return false;
            }
            if (ssh_options_set(m_session, SSH_OPTIONS_PROXYJUMP, proxyJump.constData()) != 0) {
                qCWarning(lcSsh) << "Failed to set ProxyJump:" << sessionErrorOf(m_session);
                return false;
            }
            registerJumpCallbacks(connection);
        }

        applyAdvancedOptions(connection);
    }

    return true;
}


void SshSession::applyAdvancedOptions(const Connection &connection)
{
    if (m_session == nullptr) {
        return;
    }

    if (connection.source == ConnectionSource::App) {
        const char *compression = connection.compressionEnabled ? "yes" : "no";
        ssh_options_set(m_session, SSH_OPTIONS_COMPRESSION, compression);
    } else if (connection.compressionEnabled) {
        ssh_options_set(m_session, SSH_OPTIONS_COMPRESSION, "yes");
    }
}


void SshSession::registerJumpCallbacks(const Connection &connection)
{
    ssh_session session = m_session;
    m_jumpContexts.clear();
    m_jumpCallbacks.clear();
    m_jumpContexts.reserve(static_cast<size_t>(connection.jumpHops.size()));
    m_jumpCallbacks.reserve(static_cast<size_t>(connection.jumpHops.size()));

    for (int i = 0; i < connection.jumpHops.size(); ++i) {
        JumpHopContext context;
        context.session = this;
        context.hopIndex = i;
        m_jumpContexts.push_back(context);

        ssh_jump_callbacks_struct callbacks{};
        callbacks.userdata = &m_jumpContexts.back();
        callbacks.before_connection = &SshSession::jumpBeforeConnectionCb;
        callbacks.verify_knownhost = &SshSession::jumpVerifyKnownHostCb;
        callbacks.authenticate = &SshSession::jumpAuthenticateCb;
        m_jumpCallbacks.push_back(callbacks);

        if (ssh_options_set(
                session, SSH_OPTIONS_PROXYJUMP_CB_LIST_APPEND, &m_jumpCallbacks.back()) != 0) {
            qCWarning(lcSsh) << "Failed to register jump callback for hop" << i << ":"
                             << sessionErrorOf(session);
        }
    }
}


bool SshSession::connectWithFallback(const Connection &connection, QString *errorOut)
{
    Q_UNUSED(connection);

    logSessionOptions("before-connect");
    if (ssh_connect(m_session) == SSH_OK) {
        return true;
    }

    const QString firstError = sessionError();
    if (errorOut) {
        *errorOut = firstError;
    }

#ifdef Q_OS_WIN
    if (firstError.contains(QLatin1String(kClientInitBufferError), Qt::CaseInsensitive)) {
        qCWarning(lcSsh) << "Retrying with Windows-compatible SSH algorithm set";

        cleanup();
        m_session = ssh_new();
        if (m_session == nullptr) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to create SSH session");
            }
            return false;
        }

        if (!applyConnectionOptions(connection)) {
            if (errorOut) {
                *errorOut = sessionError();
            }
            return false;
        }

        applyWindowsAlgorithmFallback();
        logSessionOptions("fallback-connect");
        if (ssh_connect(m_session) == SSH_OK) {
            return true;
        }

        if (errorOut) {
            *errorOut = sessionError();
        }
    }
#endif

    return false;
}


void SshSession::applyWindowsAlgorithmFallback()
{
#ifdef Q_OS_WIN
    if (m_session == nullptr) {
        return;
    }

    static constexpr auto kKex =
        "curve25519-sha256,ecdh-sha2-nistp256,diffie-hellman-group14-sha256";
    static constexpr auto kHostKeys = "ssh-ed25519,ecdsa-sha2-nistp256,rsa-sha2-512,rsa-sha2-256";
    static constexpr auto kCiphers =
        "chacha20-poly1305@openssh.com,aes256-ctr,aes192-ctr,aes128-ctr,aes256-gcm@openssh.com,"
        "aes128-gcm@openssh.com";

    ssh_options_set(m_session, SSH_OPTIONS_KEY_EXCHANGE, kKex);
    ssh_options_set(m_session, SSH_OPTIONS_HOSTKEYS, kHostKeys);
    ssh_options_set(m_session, SSH_OPTIONS_CIPHERS_C_S, kCiphers);
    ssh_options_set(m_session, SSH_OPTIONS_CIPHERS_S_C, kCiphers);
#endif
}


void SshSession::logSessionOptions(const char *stage) const
{
    if (m_session == nullptr) {
        return;
    }

    unsigned int port = 0;
    const bool hasPort = ssh_options_get_port(m_session, &port) == SSH_OK;
    const QString host = optionString(m_session, SSH_OPTIONS_HOST);
    const QString user = optionString(m_session, SSH_OPTIONS_USER);
    const QString identity = optionString(m_session, SSH_OPTIONS_IDENTITY);
    const QString kex = optionString(m_session, SSH_OPTIONS_KEY_EXCHANGE);
    const QString hostKeys = optionString(m_session, SSH_OPTIONS_HOSTKEYS);
    const QString c2s = optionString(m_session, SSH_OPTIONS_CIPHERS_C_S);
    const QString s2c = optionString(m_session, SSH_OPTIONS_CIPHERS_S_C);
    const QString compression = optionString(m_session, SSH_OPTIONS_COMPRESSION);

    qCWarning(lcSsh).noquote()
        << QStringLiteral("Session options [%1] host=%2 user=%3 port=%4 identity=%5 kex=%6 "
                          "hostkeys=%7 ciphers_c2s=%8 ciphers_s2c=%9 compression=%10")
               .arg(QString::fromUtf8(stage ? stage : "unknown"),
                    host.isEmpty() ? QStringLiteral("<unset>") : host,
                    user.isEmpty() ? QStringLiteral("<unset>") : user,
                    hasPort ? QString::number(port) : QStringLiteral("<unset>"),
                    identity.isEmpty() ? QStringLiteral("<unset>") : identity,
                    kex.isEmpty() ? QStringLiteral("<default>") : kex,
                    hostKeys.isEmpty() ? QStringLiteral("<default>") : hostKeys,
                    c2s.isEmpty() ? QStringLiteral("<default>") : c2s,
                    s2c.isEmpty() ? QStringLiteral("<default>") : s2c,
                    compression.isEmpty() ? QStringLiteral("<default>") : compression);
}


int SshSession::jumpBeforeConnectionCb(ssh_session session, void *userdata)
{
    auto *context = static_cast<JumpHopContext *>(userdata);
    if (context == nullptr || context->session == nullptr) {
        return SSH_ERROR;
    }
    return context->session->handleJumpBeforeConnection(session, context->hopIndex);
}

int SshSession::jumpVerifyKnownHostCb(ssh_session session, void *userdata)
{
    auto *context = static_cast<JumpHopContext *>(userdata);
    if (context == nullptr || context->session == nullptr) {
        return SSH_ERROR;
    }
    return context->session->handleJumpVerifyKnownHost(session, context->hopIndex);
}

int SshSession::jumpAuthenticateCb(ssh_session session, void *userdata)
{
    auto *context = static_cast<JumpHopContext *>(userdata);
    if (context == nullptr || context->session == nullptr) {
        return SSH_ERROR;
    }
    return context->session->handleJumpAuthenticate(session, context->hopIndex);
}

int SshSession::handleJumpBeforeConnection(ssh_session session, int hopIndex)
{
    if (hopIndex < 0 || hopIndex >= m_connection.jumpHops.size()) {
        return SSH_ERROR;
    }

    const JumpHop &hop = m_connection.jumpHops.at(hopIndex);
    qCWarning(lcSsh) << "Connecting to gateway hop" << (hopIndex + 1)
                     << hop.username + QLatin1Char('@') + hop.host << "port" << hop.port;
    Q_UNUSED(session);
    return SSH_OK;
}

int SshSession::handleJumpVerifyKnownHost(ssh_session session, int hopIndex)
{
    if (hopIndex < 0 || hopIndex >= m_connection.jumpHops.size()) {
        return SSH_ERROR;
    }

    const JumpHop &hop = m_connection.jumpHops.at(hopIndex);
    const QString context = trSession("Gateway (%1@%2:%3)").arg(hop.username, hop.host).arg(hop.port);

    if (m_hostKeyVerifier && m_hostKeyVerifier(session, context)) {
        return SSH_OK;
    }

    qCWarning(lcSsh) << "Gateway host key verification failed for" << hop.username << hop.host;
    return SSH_ERROR;
}

int SshSession::handleJumpAuthenticate(ssh_session session, int hopIndex)
{
    if (hopIndex < 0 || hopIndex >= m_connection.jumpHops.size()) {
        return SSH_ERROR;
    }

    const JumpHop &hop = m_connection.jumpHops.at(hopIndex);
    Connection authProfile = m_connection;
    authProfile.username = hop.username;
    authProfile.authType = hop.authType;
    authProfile.privateKeyPath = hop.privateKeyPath;

    QString secret;
    if (hop.useTargetCredentials || hopIndex > 0) {
        secret = m_credentials.targetSecret;
    } else {
        secret = m_credentials.gatewaySecret;
    }

    if (SshAuth::authenticateSession(session, authProfile, secret)) {
        return SSH_OK;
    }

    qCWarning(lcSsh) << "Gateway authentication failed for" << hop.username << hop.host << ":"
                     << sessionErrorOf(session);
    return SSH_ERROR;
}


bool SshSession::openShell(int cols, int rows, QString *errorOut)
{
    m_channel = ssh_channel_new(m_session);
    if (m_channel == nullptr) {
        if (errorOut) {
            *errorOut = trSession("Failed to create channel: %1").arg(sessionError());
        }
        return false;
    }

    if (ssh_channel_open_session(m_channel) != SSH_OK) {
        if (errorOut) {
            *errorOut = trSession("Failed to open channel: %1").arg(sessionError());
        }
        return false;
    }

    if (ssh_channel_request_pty_size(m_channel, "xterm-256color", cols, rows) != SSH_OK) {
        if (errorOut) {
            *errorOut = trSession("Failed to request PTY: %1").arg(sessionError());
        }
        return false;
    }

    if (ssh_channel_request_shell(m_channel) != SSH_OK) {
        if (errorOut) {
            *errorOut = trSession("Failed to request shell: %1").arg(sessionError());
        }
        return false;
    }

    return true;
}


bool SshSession::pollKeepAlive(bool hadChannelActivity, QString *errorOut)
{
    if (m_keepAliveIntervalSec <= 0 || m_session == nullptr) {
        return true;
    }

    if (hadChannelActivity) {
        m_keepAliveMissed = 0;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastKeepAliveMs > 0 &&
        now - m_lastKeepAliveMs < static_cast<qint64>(m_keepAliveIntervalSec) * 1000) {
        return true;
    }

    m_lastKeepAliveMs = now;

    if (ssh_send_keepalive(m_session) != SSH_OK || !ssh_is_connected(m_session)) {
        ++m_keepAliveMissed;
        if (m_keepAliveMissed >= m_keepAliveCountMax) {
            if (errorOut) {
                *errorOut = trSession("Connection lost (keepalive)");
            }
            return false;
        }
        return true;
    }

    if (hadChannelActivity) {
        m_keepAliveMissed = 0;
    }
    return true;
}


bool SshSession::writeToChannel(const QByteArray &data, QString *errorOut)
{
    if (m_channel == nullptr || data.isEmpty()) {
        return true;
    }

    const char *ptr = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        const int written = ssh_channel_write(m_channel, ptr, static_cast<uint32_t>(remaining));
        if (written == SSH_ERROR || written < 0) {
            if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel) ||
                (m_session && !ssh_is_connected(m_session))) {
                return false;
            }
            if (errorOut) {
                *errorOut = trSession("Failed to write to channel: %1").arg(sessionError());
            }
            return false;
        }
        if (written == 0) {
            break;
        }
        ptr += written;
        remaining -= written;
    }
    return true;
}

bool SshSession::changePtySize(int cols, int rows, QString *errorOut)
{
    Q_UNUSED(errorOut);
    if (m_channel == nullptr) {
        return true;
    }
    if (cols < 2 || rows < 2 || cols > 1000 || rows > 500) {
        return true;
    }
    if (ssh_channel_change_pty_size(m_channel, cols, rows) != SSH_OK) {
        return true;
    }
    return true;
}

SshSession::ShellPollStatus SshSession::pollShell(QByteArray *outData, QString *errorOut)
{
    if (m_session == nullptr) {
        return ShellPollStatus::Disconnected;
    }
    if (m_channel == nullptr) {
        return ShellPollStatus::Idle;
    }
    if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel)) {
        return ShellPollStatus::Disconnected;
    }

    constexpr int kMaxBytesPerTick = 64 * 1024;
    int totalRead = 0;
    char buffer[4096];
    bool hadData = false;

    auto readStream = [&](int isStderr) -> ShellPollStatus {
        while (totalRead < kMaxBytesPerTick) {
            const int nbytes =
                ssh_channel_read_nonblocking(m_channel, buffer, sizeof(buffer), isStderr);
            if (nbytes == SSH_EOF) {
                return ShellPollStatus::Disconnected;
            }
            if (nbytes < 0) {
                if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel) ||
                    !ssh_is_connected(m_session)) {
                    return ShellPollStatus::Disconnected;
                }
                if (errorOut) {
                    *errorOut = trSession("Read error: %1").arg(sessionError());
                }
                return ShellPollStatus::Error;
            }
            if (nbytes == 0) {
                break;
            }
            totalRead += nbytes;
            hadData = true;
            if (outData) {
                outData->append(buffer, nbytes);
            }
        }
        return ShellPollStatus::Idle;
    };

    ShellPollStatus st = readStream(0);
    if (st != ShellPollStatus::Idle) {
        return st;
    }
    st = readStream(1);
    if (st != ShellPollStatus::Idle) {
        return st;
    }

    if (!ssh_channel_is_open(m_channel) || ssh_channel_is_eof(m_channel) ||
        !ssh_is_connected(m_session)) {
        return ShellPollStatus::Disconnected;
    }

    return hadData ? ShellPollStatus::Data : ShellPollStatus::Idle;
}
