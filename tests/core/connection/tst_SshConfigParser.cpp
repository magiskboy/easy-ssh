// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/connection/SshConfigParser.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class SshConfigParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void stableIdIsDeterministic();
    void parseProxyJumpHopsWithoutConfig();
    void loadResolvesAliasIncludeAndOpaqueKeywords();
};

void SshConfigParserTest::stableIdIsDeterministic()
{
    const QUuid first = SshConfigParser::stableIdForAlias(QStringLiteral("lab"));
    const QUuid second = SshConfigParser::stableIdForAlias(QStringLiteral("lab"));
    const QUuid other = SshConfigParser::stableIdForAlias(QStringLiteral("prod"));
    QCOMPARE(first, second);
    QVERIFY(first != other);
}

void SshConfigParserTest::parseProxyJumpHopsWithoutConfig()
{
    const QList<JumpHop> hops = SshConfigParser::parseProxyJumpHops(
        ProxyJumpParseRequest{QStringLiteral("alice@jump.example:2222,bare-host"), QString()});
    QCOMPARE(hops.size(), 2);
    QCOMPARE(hops.at(0).host, QStringLiteral("jump.example"));
    QCOMPARE(hops.at(0).port, quint16(2222));
    QCOMPARE(hops.at(0).username, QStringLiteral("alice"));
    QCOMPARE(hops.at(1).host, QStringLiteral("bare-host"));
    QCOMPARE(hops.at(1).port, quint16(22));
}

void SshConfigParserTest::loadResolvesAliasIncludeAndOpaqueKeywords()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString includePath = temp.filePath(QStringLiteral("extra.conf"));
    {
        QFile file(includePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("Host bastion\n"
                   "  HostName bastion.internal\n"
                   "  User jump\n"
                   "  Port 2201\n"
                   "  IdentityFile ~/.ssh/id_ed25519\n");
    }

    const QString configPath = temp.filePath(QStringLiteral("config"));
    {
        QFile file(configPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(QByteArray("Include ") + QFile::encodeName(includePath) +
                   "\n"
                   "Host *\n"
                   "  ForwardAgent no\n"
                   "Host lab\n"
                   "  HostName lab.example\n"
                   "  User alice\n"
                   "  Port 2222\n"
                   "  ProxyJump bastion\n"
                   "  ForwardAgent yes\n"
                   "Host *.wildcard\n"
                   "  HostName ignored\n");
    }

    const QList<SshConfigHost> hosts = SshConfigParser::load(configPath);
    QCOMPARE(hosts.size(), 2);

    SshConfigHost lab;
    SshConfigHost bastion;
    for (const SshConfigHost &host : hosts) {
        if (host.alias == QLatin1String("lab")) {
            lab = host;
        } else if (host.alias == QLatin1String("bastion")) {
            bastion = host;
        }
    }

    QCOMPARE(lab.alias, QStringLiteral("lab"));
    QCOMPARE(lab.hostName, QStringLiteral("lab.example"));
    QCOMPARE(lab.user, QStringLiteral("alice"));
    QCOMPARE(lab.port, quint16(2222));
    QCOMPARE(lab.proxyJump, QStringLiteral("bastion"));
    QVERIFY(lab.forwardAgent);

    QCOMPARE(bastion.hostName, QStringLiteral("bastion.internal"));
    QCOMPARE(bastion.user, QStringLiteral("jump"));
    QCOMPARE(bastion.port, quint16(2201));

    const QList<Connection> connections = SshConfigParser::toConnections(hosts, configPath);
    QCOMPARE(connections.size(), 2);

    Connection labConnection;
    for (const Connection &connection : connections) {
        if (connection.configAlias == QLatin1String("lab")) {
            labConnection = connection;
            break;
        }
    }
    QCOMPARE(labConnection.source, ConnectionSource::SshConfig);
    QCOMPARE(labConnection.proxyMode, SshProxyMode::ProxyJump);
    QVERIFY(labConnection.agentForwarding);
    QCOMPARE(labConnection.jumpHops.size(), 1);
    QCOMPARE(labConnection.jumpHops.first().host, QStringLiteral("bastion.internal"));
    QCOMPARE(labConnection.jumpHops.first().port, quint16(2201));
    QCOMPARE(labConnection.jumpHops.first().username, QStringLiteral("jump"));
}

QTEST_GUILESS_MAIN(SshConfigParserTest)

#include "tst_SshConfigParser.moc"
