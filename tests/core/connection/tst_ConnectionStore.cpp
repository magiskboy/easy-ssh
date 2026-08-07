// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/connection/Connection.h"
#include "core/connection/ConnectionStore.h"

#include <QCoreApplication>
#include <QSettings>
#include <QUuid>
#include <QtTest>

class ConnectionStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void roundTripPreservesFullConnection();
    void roundTripPreservesSourceAndConfigAlias();
};

void ConnectionStoreTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("easy-ssh-test"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_ConnectionStore"));
}

void ConnectionStoreTest::cleanup()
{
    QSettings settings;
    settings.clear();
    settings.sync();
}

void ConnectionStoreTest::roundTripPreservesFullConnection()
{
    Connection original;
    original.id = QUuid::createUuid();
    original.name = QStringLiteral("lab");
    original.host = QStringLiteral("lab.example");
    original.port = 2222;
    original.username = QStringLiteral("alice");
    original.authType = AuthType::PrivateKey;
    original.savePassword = false;
    original.privateKeyPath = QStringLiteral("/tmp/id_ed25519");
    original.startupDirectory = QStringLiteral("/var/www");
    original.source = ConnectionSource::App;
    original.configAlias = QStringLiteral("lab");
    original.proxyMode = SshProxyMode::ProxyJump;
    original.keepAliveIntervalSec = 30;
    original.keepAliveCountMax = 5;
    original.compressionEnabled = true;
    original.agentForwarding = true;
    original.shellCommands.shell = QStringLiteral("/bin/bash");
    original.shellCommands.listingCommand = QStringLiteral("ls -la");
    original.shellCommands.allowScpFallback = false;
    original.shellCommands.ignoreLsWarnings = true;

    JumpHop hop;
    hop.host = QStringLiteral("bastion.example");
    hop.port = 2201;
    hop.username = QStringLiteral("jump");
    hop.authType = AuthType::Password;
    hop.useTargetCredentials = false;
    original.jumpHops.append(hop);

    ConnectionStore::save({original});
    const QList<Connection> loaded = ConnectionStore::load();
    QCOMPARE(loaded.size(), 1);

    const Connection &c = loaded.first();
    QCOMPARE(c.id, original.id);
    QCOMPARE(c.name, original.name);
    QCOMPARE(c.host, original.host);
    QCOMPARE(c.port, original.port);
    QCOMPARE(c.username, original.username);
    QCOMPARE(c.authType, AuthType::PrivateKey);
    QCOMPARE(c.savePassword, false);
    QCOMPARE(c.privateKeyPath, original.privateKeyPath);
    QCOMPARE(c.startupDirectory, original.startupDirectory);
    QCOMPARE(c.source, ConnectionSource::App);
    QCOMPARE(c.configAlias, original.configAlias);
    QCOMPARE(c.proxyMode, SshProxyMode::ProxyJump);
    QCOMPARE(c.keepAliveIntervalSec, 30);
    QCOMPARE(c.keepAliveCountMax, 5);
    QCOMPARE(c.compressionEnabled, true);
    QCOMPARE(c.agentForwarding, true);
    QCOMPARE(c.shellCommands.shell, QStringLiteral("/bin/bash"));
    QCOMPARE(c.shellCommands.listingCommand, QStringLiteral("ls -la"));
    QCOMPARE(c.shellCommands.allowScpFallback, false);
    QCOMPARE(c.shellCommands.ignoreLsWarnings, true);
    QCOMPARE(c.jumpHops.size(), 1);
    QCOMPARE(c.jumpHops.first().host, QStringLiteral("bastion.example"));
    QCOMPARE(c.jumpHops.first().port, quint16(2201));
    QCOMPARE(c.jumpHops.first().username, QStringLiteral("jump"));
    QCOMPARE(c.jumpHops.first().useTargetCredentials, false);
    QVERIFY(c.proxyCommand.isEmpty());
}

void ConnectionStoreTest::roundTripPreservesSourceAndConfigAlias()
{
    Connection original;
    original.id = QUuid::createUuid();
    original.name = QStringLiteral("from-config");
    original.host = QStringLiteral("host.example");
    original.username = QStringLiteral("bob");
    original.source = ConnectionSource::SshConfig;
    original.configAlias = QStringLiteral("prod-alias");

    ConnectionStore::save({original});
    const QList<Connection> loaded = ConnectionStore::load();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().source, ConnectionSource::SshConfig);
    QCOMPARE(loaded.first().configAlias, QStringLiteral("prod-alias"));
}

QTEST_GUILESS_MAIN(ConnectionStoreTest)

#include "tst_ConnectionStore.moc"
