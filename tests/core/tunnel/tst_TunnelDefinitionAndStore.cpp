/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/tunnel/Tunnel.h"
#include "core/tunnel/TunnelStore.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class TunnelDefinitionAndStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void validationAndAddresses();
    void storeRoundTripAndFiltering();

private:
    QTemporaryDir m_temp;
};

void TunnelDefinitionAndStoreTest::initTestCase()
{
    QVERIFY(m_temp.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_temp.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_temp.path());
    QCoreApplication::setOrganizationName(QStringLiteral("EasySshTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TunnelStoreTest"));
}

void TunnelDefinitionAndStoreTest::cleanup()
{
    TunnelStore::save({});
}

void TunnelDefinitionAndStoreTest::validationAndAddresses()
{
    TunnelDefinition local;
    local.id = QUuid::createUuid();
    local.connectionId = QUuid::createUuid();
    local.name = QStringLiteral("db");
    local.type = TunnelType::Local;
    local.localHost = QStringLiteral("127.0.0.1");
    local.localPort = 5432;
    local.remoteHost = QStringLiteral("127.0.0.1");
    local.remotePort = 5432;
    QVERIFY(local.isValid());
    QCOMPARE(local.localAddress(), QStringLiteral("127.0.0.1:5432"));
    QCOMPARE(local.remoteAddress(), QStringLiteral("127.0.0.1:5432"));

    TunnelDefinition dynamic = local;
    dynamic.type = TunnelType::Dynamic;
    dynamic.name = QStringLiteral("socks");
    QVERIFY(dynamic.isValid());
    QCOMPARE(dynamic.remoteAddress(), QStringLiteral("SOCKS5"));
    dynamic.socksAuth = SocksAuthMode::UsernamePassword;
    QVERIFY(!dynamic.isValid());
    dynamic.socksUsername = QStringLiteral("user");
    QVERIFY(dynamic.isValid());

    TunnelDefinition unixLocal = local;
    unixLocal.localKind = TunnelEndpointKind::UnixSocket;
    unixLocal.localSocketPath = QStringLiteral("relative.sock");
    QVERIFY(!unixLocal.isValid());
    unixLocal.localSocketPath = QStringLiteral("/tmp/db.sock");
    QVERIFY(unixLocal.isValid());
    QCOMPARE(unixLocal.localAddress(), QStringLiteral("/tmp/db.sock"));

    TunnelDefinition remote = local;
    remote.type = TunnelType::Remote;
    remote.name.clear();
    QCOMPARE(remote.validationError(), QStringLiteral("Name is required"));
}

void TunnelDefinitionAndStoreTest::storeRoundTripAndFiltering()
{
    const QUuid connectionA = QUuid::createUuid();
    const QUuid connectionB = QUuid::createUuid();

    TunnelDefinition local;
    local.id = QUuid::createUuid();
    local.connectionId = connectionA;
    local.name = QStringLiteral("local");
    local.type = TunnelType::Local;
    local.localPort = 8080;
    local.remotePort = 80;

    TunnelDefinition dynamic;
    dynamic.id = QUuid::createUuid();
    dynamic.connectionId = connectionA;
    dynamic.name = QStringLiteral("socks");
    dynamic.type = TunnelType::Dynamic;
    dynamic.localPort = 1080;
    dynamic.socksAuth = SocksAuthMode::UsernamePassword;
    dynamic.socksUsername = QStringLiteral("alice");
    dynamic.socksPassword = QStringLiteral("secret");
    dynamic.remoteSocketPath = QStringLiteral("/should/clear");
    dynamic.localSocketPath = QStringLiteral("/should/clear");

    TunnelDefinition other;
    other.id = QUuid::createUuid();
    other.connectionId = connectionB;
    other.name = QStringLiteral("other");
    other.type = TunnelType::Remote;
    other.localPort = 9000;
    other.remotePort = 9000;

    TunnelDefinition invalid;
    invalid.id = QUuid::createUuid();
    invalid.connectionId = connectionA;
    invalid.name = QStringLiteral("bad");
    invalid.localPort = 0;

    TunnelStore::save({local, dynamic, other, invalid});

    const QList<TunnelDefinition> all = TunnelStore::load();
    QCOMPARE(all.size(), 3);

    bool sawDynamic = false;
    for (const TunnelDefinition &tunnel : all) {
        if (tunnel.name == QLatin1String("socks")) {
            sawDynamic = true;
            QCOMPARE(tunnel.socksAuth, SocksAuthMode::UsernamePassword);
            QCOMPARE(tunnel.socksUsername, QStringLiteral("alice"));
            QVERIFY(tunnel.socksPassword.isEmpty());
            QVERIFY(tunnel.remoteSocketPath.isEmpty());
            QVERIFY(tunnel.localSocketPath.isEmpty());
            QCOMPARE(tunnel.localKind, TunnelEndpointKind::Tcp);
        }
    }
    QVERIFY(sawDynamic);

    QCOMPARE(TunnelStore::loadForConnection(connectionA).size(), 2);
    TunnelStore::removeByConnectionId(connectionA);
    QCOMPARE(TunnelStore::load().size(), 1);
    QCOMPARE(TunnelStore::load().first().connectionId, connectionB);
}

QTEST_GUILESS_MAIN(TunnelDefinitionAndStoreTest)

#include "tst_TunnelDefinitionAndStore.moc"
