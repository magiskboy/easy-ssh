#include "SshKnownHosts.h"

#include "core/util/Logging.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>

namespace
{
QString trKh(const char *text)
{
    return QCoreApplication::translate("SshKnownHosts", text);
}

QString sessionErrorOf(ssh_session session)
{
    if (session == nullptr) {
        return trKh("Unknown error");
    }
    const char *err = ssh_get_error(session);
    return err ? QString::fromUtf8(err) : trKh("Unknown error");
}
} // namespace

QString SshKnownHosts::fingerprintOf(ssh_session session)
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

QString SshKnownHosts::knownHostsFilePathFor(ssh_session session)
{
    if (session != nullptr) {
        char *path = nullptr;
        if (ssh_options_get(session, SSH_OPTIONS_KNOWNHOSTS, &path) == SSH_OK && path != nullptr) {
            const QString result = QString::fromUtf8(path);
            ssh_string_free_char(path);
            if (!result.isEmpty()) {
                return result;
            }
        }
    }
    return QDir::home().filePath(QStringLiteral(".ssh/known_hosts"));
}

bool SshKnownHosts::knownHostsLineMatchesHost(const QString &hostField, const QString &host, int port)
{
    if (hostField.isEmpty() || host.isEmpty()) {
        return false;
    }
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

bool SshKnownHosts::removeKnownHostsEntriesForSession(ssh_session session)
{
    if (session == nullptr) {
        return false;
    }

    char *hostC = nullptr;
    if (ssh_options_get(session, SSH_OPTIONS_HOST, &hostC) != SSH_OK || hostC == nullptr) {
        return false;
    }
    const QString host = QString::fromUtf8(hostC);
    ssh_string_free_char(hostC);

    unsigned int port = 22;
    ssh_options_get_port(session, &port);

    const QString path = knownHostsFilePathFor(session);
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

bool SshKnownHosts::verify(ssh_session session,
                           const QString &contextLabel,
                           const TrustCallback &trustCallback,
                           const ErrorCallback &onError)
{
    if (session == nullptr) {
        return false;
    }

    const auto fail = [&](const QString &message) {
        if (contextLabel.isEmpty() && onError) {
            onError(message);
        }
        return false;
    };

    const auto promptAndUpdate = [&](Disposition disposition, bool removeExisting) -> bool {
        const QString fingerprint = fingerprintOf(session);
        if (fingerprint.isEmpty()) {
            return fail(trKh("Unable to read server host key fingerprint"));
        }

        qCWarning(lcSsh) << "Host key prompt" << static_cast<int>(disposition) << fingerprint
                         << contextLabel;

        if (!trustCallback || !trustCallback(disposition, fingerprint, contextLabel)) {
            qCWarning(lcSsh) << "Host key rejected by user";
            return fail(trKh("Host key was rejected"));
        }

        if (removeExisting && !removeKnownHostsEntriesForSession(session)) {
            return fail(trKh("Failed to update known_hosts: could not remove old host key"));
        }

        if (ssh_session_update_known_hosts(session) != SSH_OK) {
            return fail(trKh("Failed to update known_hosts: %1").arg(sessionErrorOf(session)));
        }

        qCWarning(lcSsh) << "Host key accepted and known_hosts updated";
        return true;
    };

    switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        return true;
    case SSH_KNOWN_HOSTS_CHANGED:
        return promptAndUpdate(Disposition::Changed, true);
    case SSH_KNOWN_HOSTS_OTHER:
        return promptAndUpdate(Disposition::Other, true);
    case SSH_KNOWN_HOSTS_ERROR:
        return fail(trKh("Error checking known hosts: %1").arg(sessionErrorOf(session)));
    case SSH_KNOWN_HOSTS_NOT_FOUND:
    case SSH_KNOWN_HOSTS_UNKNOWN:
        return promptAndUpdate(Disposition::Unknown, false);
    default:
        return fail(trKh("Unexpected known_hosts state"));
    }
}
