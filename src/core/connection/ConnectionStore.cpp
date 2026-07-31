// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ConnectionStore.h"

#include <QSettings>

namespace
{

QString authTypeToString(AuthType type)
{
    switch (type) {
    case AuthType::PrivateKey:
        return QStringLiteral("privateKey");
    case AuthType::Password:
    default:
        return QStringLiteral("password");
    }
}

AuthType authTypeFromString(const QString &value)
{
    if (value == QLatin1String("privateKey")) {
        return AuthType::PrivateKey;
    }
    return AuthType::Password;
}

void loadJumpHops(QSettings &settings, Connection &connection)
{
    connection.jumpHops.clear();
    const int hopCount = settings.beginReadArray(QStringLiteral("jumpHops"));
    connection.jumpHops.reserve(hopCount);

    for (int i = 0; i < hopCount; ++i) {
        settings.setArrayIndex(i);
        JumpHop hop;
        hop.host = settings.value(QStringLiteral("host")).toString();
        hop.port = static_cast<quint16>(settings.value(QStringLiteral("port"), 22).toUInt());
        hop.username = settings.value(QStringLiteral("username")).toString();
        hop.authType = authTypeFromString(settings.value(QStringLiteral("authType")).toString());
        hop.privateKeyPath = settings.value(QStringLiteral("privateKeyPath")).toString();
        hop.useTargetCredentials =
            settings.value(QStringLiteral("useTargetCredentials"), true).toBool();
        connection.jumpHops.append(hop);
    }

    settings.endArray();
}

void saveJumpHops(QSettings &settings, const Connection &connection)
{
    settings.beginWriteArray(QStringLiteral("jumpHops"), connection.jumpHops.size());
    for (int i = 0; i < connection.jumpHops.size(); ++i) {
        settings.setArrayIndex(i);
        const JumpHop &hop = connection.jumpHops.at(i);
        settings.setValue(QStringLiteral("host"), hop.host);
        settings.setValue(QStringLiteral("port"), hop.port);
        settings.setValue(QStringLiteral("username"), hop.username);
        settings.setValue(QStringLiteral("authType"), authTypeToString(hop.authType));
        settings.setValue(QStringLiteral("privateKeyPath"), hop.privateKeyPath);
        settings.setValue(QStringLiteral("useTargetCredentials"), hop.useTargetCredentials);
    }
    settings.endArray();
}

} // namespace

QList<Connection> ConnectionStore::load()
{
    QSettings settings;
    QList<Connection> connections;

    const int size = settings.beginReadArray(QStringLiteral("connections"));
    connections.reserve(size);

    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        Connection connection;
        connection.id = QUuid::fromString(settings.value(QStringLiteral("id")).toString());
        connection.name = settings.value(QStringLiteral("name")).toString();
        connection.host = settings.value(QStringLiteral("host")).toString();
        connection.port = static_cast<quint16>(settings.value(QStringLiteral("port"), 22).toUInt());
        connection.username = settings.value(QStringLiteral("username")).toString();
        connection.authType =
            authTypeFromString(settings.value(QStringLiteral("authType")).toString());
        connection.privateKeyPath = settings.value(QStringLiteral("privateKeyPath")).toString();
        connection.startupDirectory = settings.value(QStringLiteral("startupDirectory")).toString();
        connection.keepAliveIntervalSec =
            settings.value(QStringLiteral("keepAliveIntervalSec"), 0).toInt();
        connection.keepAliveCountMax =
            settings.value(QStringLiteral("keepAliveCountMax"), 3).toInt();
        connection.compressionEnabled =
            settings.value(QStringLiteral("compressionEnabled"), false).toBool();

        settings.beginGroup(QStringLiteral("shellCommands"));
        connection.shellCommands.shell = settings.value(QStringLiteral("shell")).toString();
        connection.shellCommands.listingCommand =
            settings.value(QStringLiteral("listingCommand")).toString();
        connection.shellCommands.listFileCommand =
            settings.value(QStringLiteral("listFileCommand")).toString();
        connection.shellCommands.mkdirCommand =
            settings.value(QStringLiteral("mkdirCommand")).toString();
        connection.shellCommands.removeCommand =
            settings.value(QStringLiteral("removeCommand")).toString();
        connection.shellCommands.renameCommand =
            settings.value(QStringLiteral("renameCommand")).toString();
        connection.shellCommands.pwdCommand =
            settings.value(QStringLiteral("pwdCommand")).toString();
        connection.shellCommands.realpathCommand =
            settings.value(QStringLiteral("realpathCommand")).toString();
        connection.shellCommands.clearAliases =
            settings.value(QStringLiteral("clearAliases"), true).toBool();
        connection.shellCommands.clearNationalVars =
            settings.value(QStringLiteral("clearNationalVars"), true).toBool();
        connection.shellCommands.tryFullTime =
            settings.value(QStringLiteral("tryFullTime"), true).toBool();
        connection.shellCommands.ignoreLsWarnings =
            settings.value(QStringLiteral("ignoreLsWarnings"), false).toBool();
        connection.shellCommands.allowScpFallback =
            settings.value(QStringLiteral("allowScpFallback"), true).toBool();
        settings.endGroup();

        loadJumpHops(settings, connection);

        if (connection.id.isNull() || connection.name.isEmpty()) {
            continue;
        }
        connections.append(connection);
    }

    settings.endArray();
    return connections;
}

void ConnectionStore::save(const QList<Connection> &connections)
{
    QSettings settings;
    settings.remove(QStringLiteral("connections"));
    settings.beginWriteArray(QStringLiteral("connections"), connections.size());

    for (int i = 0; i < connections.size(); ++i) {
        const Connection &connection = connections.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), connection.id.toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("name"), connection.name);
        settings.setValue(QStringLiteral("host"), connection.host);
        settings.setValue(QStringLiteral("port"), connection.port);
        settings.setValue(QStringLiteral("username"), connection.username);
        settings.setValue(QStringLiteral("authType"), authTypeToString(connection.authType));
        settings.setValue(QStringLiteral("privateKeyPath"), connection.privateKeyPath);
        settings.setValue(QStringLiteral("startupDirectory"), connection.startupDirectory);
        settings.setValue(QStringLiteral("keepAliveIntervalSec"), connection.keepAliveIntervalSec);
        settings.setValue(QStringLiteral("keepAliveCountMax"), connection.keepAliveCountMax);
        settings.setValue(QStringLiteral("compressionEnabled"), connection.compressionEnabled);

        settings.beginGroup(QStringLiteral("shellCommands"));
        settings.setValue(QStringLiteral("shell"), connection.shellCommands.shell);
        settings.setValue(QStringLiteral("listingCommand"),
                          connection.shellCommands.listingCommand);
        settings.setValue(QStringLiteral("listFileCommand"),
                          connection.shellCommands.listFileCommand);
        settings.setValue(QStringLiteral("mkdirCommand"), connection.shellCommands.mkdirCommand);
        settings.setValue(QStringLiteral("removeCommand"), connection.shellCommands.removeCommand);
        settings.setValue(QStringLiteral("renameCommand"), connection.shellCommands.renameCommand);
        settings.setValue(QStringLiteral("pwdCommand"), connection.shellCommands.pwdCommand);
        settings.setValue(QStringLiteral("realpathCommand"),
                          connection.shellCommands.realpathCommand);
        settings.setValue(QStringLiteral("clearAliases"), connection.shellCommands.clearAliases);
        settings.setValue(QStringLiteral("clearNationalVars"),
                          connection.shellCommands.clearNationalVars);
        settings.setValue(QStringLiteral("tryFullTime"), connection.shellCommands.tryFullTime);
        settings.setValue(QStringLiteral("ignoreLsWarnings"),
                          connection.shellCommands.ignoreLsWarnings);
        settings.setValue(QStringLiteral("allowScpFallback"),
                          connection.shellCommands.allowScpFallback);
        settings.endGroup();

        saveJumpHops(settings, connection);
    }

    settings.endArray();
    settings.sync();
}
