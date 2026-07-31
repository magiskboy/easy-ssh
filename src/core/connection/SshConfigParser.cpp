// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SshConfigParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSet>
#include <QTextStream>

#include <libssh/libssh.h>

namespace
{

constexpr int kMaxIncludeDepth = 16;

// Fixed namespace for QUuid::createUuidV5 — stable across app restarts.
const QUuid kSshConfigNamespace =
    QUuid::fromString(QStringLiteral("a7e5c3f1-8b2d-4e6a-9c1f-0d4b8a6e2f35"));

bool isConcreteHostToken(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }
    return !token.contains(QLatin1Char('*')) && !token.contains(QLatin1Char('?'));
}

QString expandTilde(const QString &path)
{
    if (path == QLatin1String("~")) {
        return QDir::homePath();
    }
    if (path.startsWith(QLatin1String("~/"))) {
        return QDir::homePath() + path.mid(1);
    }
    return path;
}

QString defaultUsername()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString user = env.value(QStringLiteral("USER"));
    if (user.isEmpty()) {
        user = env.value(QStringLiteral("USERNAME"));
    }
    return user;
}

QStringList splitConfigTokens(const QString &line)
{
    QStringList tokens;
    QString current;
    bool inQuotes = false;
    QChar quoteChar;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (inQuotes) {
            if (ch == quoteChar) {
                inQuotes = false;
            } else {
                current.append(ch);
            }
            continue;
        }
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
            inQuotes = true;
            quoteChar = ch;
            continue;
        }
        if (ch.isSpace() || ch == QLatin1Char('=')) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            continue;
        }
        current.append(ch);
    }
    if (!current.isEmpty()) {
        tokens.append(current);
    }
    return tokens;
}

QString normalizeConfigPath(const QString &path, const QDir &relativeToDir)
{
    QString expanded = expandTilde(path);
    if (QFileInfo(expanded).isRelative()) {
        expanded = relativeToDir.absoluteFilePath(expanded);
    }
    return QFileInfo(expanded).canonicalFilePath().isEmpty()
               ? QFileInfo(expanded).absoluteFilePath()
               : QFileInfo(expanded).canonicalFilePath();
}

JumpHop parseProxyJumpEntry(const QString &entry)
{
    JumpHop hop;
    hop.useTargetCredentials = true;

    QString trimmed = entry.trimmed();
    if (trimmed.isEmpty()) {
        return hop;
    }

    QString user;
    const int at = trimmed.indexOf(QLatin1Char('@'));
    if (at >= 0) {
        user = trimmed.left(at);
        trimmed = trimmed.mid(at + 1);
    }

    const int colon = trimmed.lastIndexOf(QLatin1Char(':'));
    if (colon > 0) {
        bool ok = false;
        const uint port = trimmed.mid(colon + 1).toUInt(&ok);
        if (ok && port > 0 && port <= 65535) {
            hop.port = static_cast<quint16>(port);
            trimmed = trimmed.left(colon);
        }
    }

    hop.host = trimmed;
    hop.username = user.isEmpty() ? defaultUsername() : user;
    return hop;
}

QStringList expandIncludePattern(const QString &pattern, const QDir &relativeToDir)
{
    const QString expanded = expandTilde(pattern);
    const QFileInfo info(QFileInfo(expanded).isRelative() ? relativeToDir.absoluteFilePath(expanded)
                                                          : expanded);

    if (!info.fileName().contains(QLatin1Char('*')) &&
        !info.fileName().contains(QLatin1Char('?'))) {
        return {info.absoluteFilePath()};
    }

    QDir dir(info.absolutePath());
    if (!dir.exists()) {
        return {};
    }

    const QStringList names =
        dir.entryList(QStringList{info.fileName()}, QDir::Files | QDir::Readable, QDir::Name);
    QStringList paths;
    paths.reserve(names.size());
    for (const QString &name : names) {
        paths.append(dir.absoluteFilePath(name));
    }
    return paths;
}

QString
readProxyOptionForAlias(const QFileInfo &configFile, const QString &alias, const QString &optionKey)
{
    const QString normalized = normalizeConfigPath(configFile.filePath(), QDir::home());
    QFile file(normalized);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QDir baseDir(QFileInfo(normalized).absolutePath());
    bool inMatchingBlock = false;
    QString value;

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const int hash = line.indexOf(QLatin1Char('#'));
        if (hash > 0 && line.at(hash - 1).isSpace()) {
            line = line.left(hash).trimmed();
            if (line.isEmpty()) {
                continue;
            }
        }

        const QStringList tokens = splitConfigTokens(line);
        if (tokens.isEmpty()) {
            continue;
        }

        const QString key = tokens.first().toLower();
        if (key == QLatin1String("host")) {
            inMatchingBlock = tokens.size() == 2 && tokens.at(1) == alias;
            continue;
        }

        if (!inMatchingBlock) {
            continue;
        }

        if (key == optionKey && tokens.size() >= 2) {
            value = tokens.mid(1).join(QLatin1Char(' '));
        } else if (key == QLatin1String("include")) {
            for (int i = 1; i < tokens.size(); ++i) {
                const QStringList included = expandIncludePattern(tokens.at(i), baseDir);
                for (const QString &includedPath : included) {
                    const QString nested =
                        readProxyOptionForAlias(QFileInfo(includedPath), alias, optionKey);
                    if (!nested.isEmpty()) {
                        value = nested;
                    }
                }
            }
        }
    }

    return value;
}

QString readProxyJumpForAlias(const QFileInfo &configFile, const QString &alias)
{
    return readProxyOptionForAlias(configFile, alias, QStringLiteral("proxyjump"));
}

QString readProxyCommandForAlias(const QFileInfo &configFile, const QString &alias)
{
    return readProxyOptionForAlias(configFile, alias, QStringLiteral("proxycommand"));
}

void collectAliasesFromFile(const QString &path,
                            QStringList *aliases,
                            QSet<QString> *visited,
                            int depth)
{
    if (aliases == nullptr || visited == nullptr || depth > kMaxIncludeDepth) {
        return;
    }

    const QString normalized = normalizeConfigPath(path, QDir::home());
    if (normalized.isEmpty() || visited->contains(normalized)) {
        return;
    }
    visited->insert(normalized);

    QFile file(normalized);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QDir baseDir(QFileInfo(normalized).absolutePath());

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const int hash = line.indexOf(QLatin1Char('#'));
        if (hash > 0 && line.at(hash - 1).isSpace()) {
            line = line.left(hash).trimmed();
            if (line.isEmpty()) {
                continue;
            }
        }

        const QStringList tokens = splitConfigTokens(line);
        if (tokens.isEmpty()) {
            continue;
        }

        const QString key = tokens.first().toLower();

        if (key == QLatin1String("include")) {
            for (int i = 1; i < tokens.size(); ++i) {
                const QStringList included = expandIncludePattern(tokens.at(i), baseDir);
                for (const QString &includedPath : included) {
                    collectAliasesFromFile(includedPath, aliases, visited, depth + 1);
                }
            }
            continue;
        }

        if (key != QLatin1String("host")) {
            continue;
        }

        // Only a single concrete Host token (no wildcards / multi-pattern).
        if (tokens.size() == 2 && isConcreteHostToken(tokens.at(1))) {
            const QString alias = tokens.at(1);
            if (!aliases->contains(alias)) {
                aliases->append(alias);
            }
        }
    }
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

SshConfigHost resolveAliasWithLibssh(const QString &alias, const QString &configPath)
{
    SshConfigHost host;
    host.alias = alias;
    host.hostName = alias;
    host.user = defaultUsername();
    host.port = 22;

    ssh_session session = ssh_new();
    if (session == nullptr) {
        return host;
    }

    const QByteArray aliasBytes = alias.toUtf8();
    ssh_options_set(session, SSH_OPTIONS_HOST, aliasBytes.constData());

    // Avoid double-processing on connect; we only need option resolution here.
    const int processConfig = 0;
    ssh_options_set(session, SSH_OPTIONS_PROCESS_CONFIG, &processConfig);

    const QByteArray pathBytes = configPath.toUtf8();
    const char *filename = configPath.isEmpty() ? nullptr : pathBytes.constData();
    if (ssh_options_parse_config(session, filename) != SSH_OK) {
        ssh_free(session);
        return host;
    }

    const QString resolvedHost = optionString(session, SSH_OPTIONS_HOST);
    if (!resolvedHost.isEmpty()) {
        host.hostName = resolvedHost;
    }

    const QString resolvedUser = optionString(session, SSH_OPTIONS_USER);
    if (!resolvedUser.isEmpty()) {
        host.user = resolvedUser;
    }

    unsigned int port = 22;
    if (ssh_options_get_port(session, &port) == SSH_OK && port > 0 && port <= 65535) {
        host.port = static_cast<quint16>(port);
    }

    const QString identity = optionString(session, SSH_OPTIONS_IDENTITY);
    if (!identity.isEmpty()) {
        QString path = identity;
        if (path.contains(QLatin1String("%d"))) {
            QString sshDir = optionString(session, SSH_OPTIONS_SSH_DIR);
            if (sshDir.isEmpty()) {
                sshDir = QDir::homePath() + QStringLiteral("/.ssh");
            }
            path.replace(QLatin1String("%d"), sshDir);
        }
        host.identityFiles.append(expandTilde(path));
    }

    const QFileInfo configInfo(configPath.isEmpty() ? SshConfigParser::defaultConfigPath()
                                                    : configPath);
    host.proxyJump = readProxyJumpForAlias(configInfo, alias);
    host.proxyCommand = readProxyCommandForAlias(configInfo, alias);

    ssh_free(session);
    return host;
}

} // namespace

QString SshConfigParser::defaultConfigPath()
{
    return QDir::homePath() + QStringLiteral("/.ssh/config");
}

QList<SshConfigHost> SshConfigParser::load(const QString &path)
{
    const QString configPath = path.isEmpty() ? defaultConfigPath() : path;

    QStringList aliases;
    QSet<QString> visited;
    collectAliasesFromFile(configPath, &aliases, &visited, 0);

    QList<SshConfigHost> hosts;
    hosts.reserve(aliases.size());

    // nullptr lets libssh apply user + system defaults the same way as OpenSSH client.
    // When an explicit path is given (tests), resolve against that file only.
    const QString resolvePath = path.isEmpty() ? QString() : configPath;

    for (const QString &alias : aliases) {
        hosts.append(resolveAliasWithLibssh(alias, resolvePath));
    }
    return hosts;
}

QList<Connection> SshConfigParser::toConnections(const QList<SshConfigHost> &hosts)
{
    QList<Connection> connections;
    connections.reserve(hosts.size());

    for (const SshConfigHost &host : hosts) {
        Connection connection;
        connection.id = stableIdForAlias(host.alias);
        connection.name = host.alias;
        connection.host = host.hostName.isEmpty() ? host.alias : host.hostName;
        connection.port = host.port;
        connection.username = host.user.isEmpty() ? defaultUsername() : host.user;
        connection.authType = AuthType::PrivateKey;
        connection.privateKeyPath =
            host.identityFiles.isEmpty() ? QString() : host.identityFiles.first();
        connection.source = ConnectionSource::SshConfig;
        connection.configAlias = host.alias;

        const bool hasJump = !host.proxyJump.isEmpty() && !isSshNoneToken(host.proxyJump);
        const bool hasCommand = !host.proxyCommand.isEmpty() && !isSshNoneToken(host.proxyCommand);
        // OpenSSH forbids both; prefer ProxyJump when both appear (misconfig).
        if (hasJump) {
            connection.proxyMode = SshProxyMode::ProxyJump;
            connection.jumpHops = SshConfigParser::parseProxyJumpHops(host.proxyJump);
            connection.proxyCommand.clear();
        } else if (hasCommand) {
            connection.proxyMode = SshProxyMode::ProxyCommand;
            connection.proxyCommand = host.proxyCommand.trimmed();
            connection.jumpHops.clear();
        } else {
            connection.proxyMode = SshProxyMode::None;
            connection.jumpHops.clear();
            connection.proxyCommand.clear();
        }

        connections.append(connection);
    }

    return connections;
}

QUuid SshConfigParser::stableIdForAlias(const QString &alias)
{
    return QUuid::createUuidV5(kSshConfigNamespace, QStringLiteral("ssh-config:") + alias);
}

QList<JumpHop> SshConfigParser::parseProxyJumpHops(const QString &proxyJump)
{
    QList<JumpHop> hops;
    for (const QString &entry : proxyJump.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const JumpHop hop = parseProxyJumpEntry(entry);
        if (!hop.host.isEmpty()) {
            hops.append(hop);
        }
    }
    return hops;
}
