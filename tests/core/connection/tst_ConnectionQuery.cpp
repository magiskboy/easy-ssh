/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/connection/ConnectionQuery.h"

#include <QtTest>

class ConnectionQueryTest final : public QObject
{
    Q_OBJECT

private slots:
    void emptyQuery();
    void userHostPortVariants();
    void ipv6AndInvalidPortFallback();
};

void ConnectionQueryTest::emptyQuery()
{
    const Connection draft = ConnectionQuery::draftFromQuery(QString());
    QVERIFY(!draft.id.isNull());
    QCOMPARE(draft.port, quint16(22));
    QVERIFY(draft.host.isEmpty());
    QVERIFY(draft.name.isEmpty());
}

void ConnectionQueryTest::userHostPortVariants()
{
    {
        const Connection draft = ConnectionQuery::draftFromQuery(QStringLiteral("  alice@box  "));
        QCOMPARE(draft.username, QStringLiteral("alice"));
        QCOMPARE(draft.host, QStringLiteral("box"));
        QCOMPARE(draft.port, quint16(22));
        QCOMPARE(draft.name, QStringLiteral("alice@box"));
    }
    {
        const Connection draft = ConnectionQuery::draftFromQuery(QStringLiteral("host:2222"));
        QCOMPARE(draft.host, QStringLiteral("host"));
        QCOMPARE(draft.port, quint16(2222));
        QCOMPARE(draft.name, QStringLiteral("host:2222"));
        QVERIFY(draft.username.isEmpty());
    }
    {
        const Connection draft =
            ConnectionQuery::draftFromQuery(QStringLiteral("bob@example.com:2200"));
        QCOMPARE(draft.username, QStringLiteral("bob"));
        QCOMPARE(draft.host, QStringLiteral("example.com"));
        QCOMPARE(draft.port, quint16(2200));
        QCOMPARE(draft.name, QStringLiteral("bob@example.com:2200"));
    }
}

void ConnectionQueryTest::ipv6AndInvalidPortFallback()
{
    {
        const Connection draft =
            ConnectionQuery::draftFromQuery(QStringLiteral("[2001:db8::1]:2222"));
        QCOMPARE(draft.host, QStringLiteral("2001:db8::1"));
        QCOMPARE(draft.port, quint16(2222));
        QCOMPARE(draft.name, QStringLiteral("2001:db8::1:2222"));
    }
    {
        const Connection draft = ConnectionQuery::draftFromQuery(QStringLiteral("[2001:db8::1]"));
        QCOMPARE(draft.host, QStringLiteral("2001:db8::1"));
        QCOMPARE(draft.port, quint16(22));
    }
    {
        const Connection draft = ConnectionQuery::draftFromQuery(QStringLiteral("box:99999"));
        QCOMPARE(draft.host, QStringLiteral("box:99999"));
        QCOMPARE(draft.name, QStringLiteral("box:99999"));
        QCOMPARE(draft.port, quint16(22));
    }
}

QTEST_GUILESS_MAIN(ConnectionQueryTest)

#include "tst_ConnectionQuery.moc"
