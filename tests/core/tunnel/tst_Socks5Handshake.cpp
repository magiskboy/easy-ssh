// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/tunnel/Socks5Handshake.h"

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class Socks5HandshakeTest final : public QObject
{
    Q_OBJECT

private slots:
    void noAuthConnectIpv4();
    void usernamePasswordSuccessAndFailure();
    void partialFramesAndUnsupportedCommand();
    void domainAndIpv6Destinations();
    void writeConnectReplyBytes();

private:
    struct SocketPair
    {
        QTcpServer server;
        QTcpSocket client;
        QTcpSocket *peer = nullptr;
    };

    bool openPair(SocketPair *pair);
    static void writeAll(QTcpSocket *socket, const QByteArray &data);
    static QByteArray waitReadable(QTcpSocket *socket, int minBytes = 1);
    static bool waitPeerReadable(QTcpSocket *peer);
};

bool Socks5HandshakeTest::openPair(SocketPair *pair)
{
    if (!pair->server.listen(QHostAddress::LocalHost)) {
        return false;
    }
    pair->client.connectToHost(QHostAddress::LocalHost, pair->server.serverPort());
    if (!pair->client.waitForConnected(1000)) {
        return false;
    }
    if (!pair->server.waitForNewConnection(1000)) {
        return false;
    }
    pair->peer = pair->server.nextPendingConnection();
    return pair->peer != nullptr;
}

void Socks5HandshakeTest::writeAll(QTcpSocket *socket, const QByteArray &data)
{
    QCOMPARE_GE(socket->write(data), qint64(data.size()));
    QVERIFY(socket->waitForBytesWritten(1000));
    socket->flush();
    QCoreApplication::processEvents();
}

QByteArray Socks5HandshakeTest::waitReadable(QTcpSocket *socket, int minBytes)
{
    QByteArray data;
    QElapsedTimer timer;
    timer.start();
    while (data.size() < minBytes && timer.elapsed() < 2000) {
        if (socket->bytesAvailable() == 0) {
            socket->waitForReadyRead(50);
            QCoreApplication::processEvents();
        }
        data.append(socket->readAll());
    }
    return data;
}

bool Socks5HandshakeTest::waitPeerReadable(QTcpSocket *peer)
{
    QElapsedTimer timer;
    timer.start();
    while (peer->bytesAvailable() == 0 && timer.elapsed() < 2000) {
        if (!peer->waitForReadyRead(50)) {
            QCoreApplication::processEvents();
        }
    }
    return peer->bytesAvailable() > 0;
}

void Socks5HandshakeTest::noAuthConnectIpv4()
{
    SocketPair pair;
    QVERIFY(openPair(&pair));

    Socks5Handshake handshake(SocksAuthMode::None, {});
    writeAll(&pair.client, QByteArray::fromHex("050100"));
    QVERIFY(waitPeerReadable(pair.peer));
    handshake.process(pair.peer);
    QCOMPARE(waitReadable(&pair.client, 2), QByteArray::fromHex("0500"));

    writeAll(&pair.client, QByteArray::fromHex("050100017f0000011f90"));
    QVERIFY(waitPeerReadable(pair.peer));
    handshake.process(pair.peer);
    QVERIFY(handshake.succeeded());
    QCOMPARE(handshake.destHost(), QStringLiteral("127.0.0.1"));
    QCOMPARE(handshake.destPort(), quint16(8080));
}

void Socks5HandshakeTest::usernamePasswordSuccessAndFailure()
{
    {
        SocketPair pair;
        QVERIFY(openPair(&pair));
        Socks5Handshake handshake(
            SocksAuthMode::UsernamePassword,
            {.username = QStringLiteral("alice"), .password = QStringLiteral("secret")});

        writeAll(&pair.client, QByteArray::fromHex("050102"));
        QVERIFY(waitPeerReadable(pair.peer));
        handshake.process(pair.peer);
        QCOMPARE(waitReadable(&pair.client, 2), QByteArray::fromHex("0502"));

        QByteArray auth;
        auth.append(char(0x01));
        auth.append(char(5));
        auth.append("alice");
        auth.append(char(6));
        auth.append("secret");
        writeAll(&pair.client, auth);
        QVERIFY(waitPeerReadable(pair.peer));
        handshake.process(pair.peer);
        QCOMPARE(waitReadable(&pair.client, 2), QByteArray::fromHex("0100"));
        QCOMPARE(handshake.state(), Socks5Handshake::State::WaitRequest);
    }

    {
        SocketPair pair;
        QVERIFY(openPair(&pair));
        Socks5Handshake handshake(
            SocksAuthMode::UsernamePassword,
            {.username = QStringLiteral("alice"), .password = QStringLiteral("secret")});
        writeAll(&pair.client, QByteArray::fromHex("050100"));
        QVERIFY(waitPeerReadable(pair.peer));
        handshake.process(pair.peer);
        QCOMPARE(waitReadable(&pair.client, 2), QByteArray::fromHex("05ff"));
        QVERIFY(handshake.isDone());
        QVERIFY(!handshake.succeeded());
    }
}

void Socks5HandshakeTest::partialFramesAndUnsupportedCommand()
{
    SocketPair pair;
    QVERIFY(openPair(&pair));
    Socks5Handshake handshake(SocksAuthMode::None, {});

    writeAll(&pair.client, QByteArray::fromHex("05"));
    QVERIFY(waitPeerReadable(pair.peer));
    handshake.process(pair.peer);
    QCOMPARE(handshake.state(), Socks5Handshake::State::WaitMethods);

    writeAll(&pair.client, QByteArray::fromHex("0100"));
    QVERIFY(waitPeerReadable(pair.peer));
    handshake.process(pair.peer);
    QCOMPARE(waitReadable(&pair.client, 2), QByteArray::fromHex("0500"));

    writeAll(&pair.client, QByteArray::fromHex("050200017f0000010050"));
    QVERIFY(waitPeerReadable(pair.peer));
    handshake.process(pair.peer);
    const QByteArray reply = waitReadable(&pair.client, 10);
    QCOMPARE(static_cast<quint8>(reply.at(1)), quint8(0x07));
    QVERIFY(!handshake.succeeded());
}

void Socks5HandshakeTest::domainAndIpv6Destinations()
{
    {
        SocketPair pair;
        QVERIFY(openPair(&pair));
        Socks5Handshake handshake(SocksAuthMode::None, {});
        writeAll(&pair.client, QByteArray::fromHex("050100"));
        QVERIFY(waitPeerReadable(pair.peer));
        handshake.process(pair.peer);
        waitReadable(&pair.client, 2);

        QByteArray request = QByteArray::fromHex("05010003");
        request.append(char(11));
        request.append("example.com");
        request.append(QByteArray::fromHex("01bb"));
        writeAll(&pair.client, request);
        QVERIFY(waitPeerReadable(pair.peer));
        handshake.process(pair.peer);
        QVERIFY(handshake.succeeded());
        QCOMPARE(handshake.destHost(), QStringLiteral("example.com"));
        QCOMPARE(handshake.destPort(), quint16(443));
    }

    {
        SocketPair pair;
        QVERIFY(openPair(&pair));
        Socks5Handshake handshake(SocksAuthMode::None, {});
        writeAll(&pair.client, QByteArray::fromHex("050100"));
        QVERIFY(waitPeerReadable(pair.peer));
        handshake.process(pair.peer);
        waitReadable(&pair.client, 2);

        writeAll(&pair.client, QByteArray::fromHex("05010004000000000000000000000000000000010050"));
        QVERIFY(waitPeerReadable(pair.peer));
        handshake.process(pair.peer);
        QVERIFY(handshake.succeeded());
        QCOMPARE(handshake.destPort(), quint16(80));
        QVERIFY(handshake.destHost().contains(QStringLiteral("::1")) ||
                handshake.destHost().contains(QStringLiteral("0:0:0:0:0:0:0:1")));
    }
}

void Socks5HandshakeTest::writeConnectReplyBytes()
{
    SocketPair pair;
    QVERIFY(openPair(&pair));
    Socks5Handshake::writeConnectReply(pair.peer, true);
    QVERIFY(pair.peer->waitForBytesWritten(1000));
    pair.peer->flush();
    QCOMPARE(waitReadable(&pair.client, 10), QByteArray::fromHex("05000001000000000000"));
    Socks5Handshake::writeConnectReply(pair.peer, false);
    QVERIFY(pair.peer->waitForBytesWritten(1000));
    pair.peer->flush();
    QCOMPARE(waitReadable(&pair.client, 10), QByteArray::fromHex("05010001000000000000"));
}

QTEST_GUILESS_MAIN(Socks5HandshakeTest)

#include "tst_Socks5Handshake.moc"
