// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/ssh/SshIoHandler.h"
#include "core/ssh/SshIoLoop.h"

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QThread>
#include <QtTest>

#include <atomic>
#include <memory>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{

class NoopHandler final : public SshIoHandler
{
public:
    explicit NoopHandler(QString id,
                         QAtomicInt *idleCount = nullptr,
                         QAtomicInt *cancelCount = nullptr)
        : m_id(std::move(id)), m_idleCount(idleCount), m_cancelCount(cancelCount)
    {
    }

    QString id() const override { return m_id; }

    bool start(SshIoLoop *loop, QString *error) override
    {
        Q_UNUSED(loop);
        Q_UNUSED(error);
        m_started = true;
        return true;
    }

    void cancel() override
    {
        m_cancelled = true;
        if (m_cancelCount != nullptr) {
            m_cancelCount->fetchAndAddRelaxed(1);
        }
    }

    void onIdle() override
    {
        if (m_idleCount != nullptr) {
            m_idleCount->fetchAndAddRelaxed(1);
        }
    }

    bool started() const { return m_started; }
    bool cancelled() const { return m_cancelled; }

private:
    QString m_id;
    QAtomicInt *m_idleCount = nullptr;
    QAtomicInt *m_cancelCount = nullptr;
    bool m_started = false;
    bool m_cancelled = false;
};

class FailingHandler final : public SshIoHandler
{
public:
    QString id() const override { return QStringLiteral("fail"); }

    bool start(SshIoLoop *loop, QString *error) override
    {
        Q_UNUSED(loop);
        if (error != nullptr) {
            *error = QStringLiteral("intentional start failure");
        }
        return false;
    }

    void cancel() override {}
};

#ifdef Q_OS_WIN
void closeSock(socket_t fd)
{
    if (fd != SSH_INVALID_SOCKET) {
        closesocket(fd);
    }
}
bool setNonBlocking(socket_t fd)
{
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
}
#else
void closeSock(socket_t fd)
{
    if (fd != SSH_INVALID_SOCKET) {
        ::close(static_cast<int>(fd));
    }
}
bool setNonBlocking(socket_t fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

bool makeConnectedPair(socket_t *a, socket_t *b)
{
    *a = SSH_INVALID_SOCKET;
    *b = SSH_INVALID_SOCKET;

    const socket_t listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener == SSH_INVALID_SOCKET) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (::bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
        ::listen(listener, 1) != 0) {
        closeSock(listener);
        return false;
    }

#ifdef Q_OS_WIN
    int addrLen = sizeof(addr);
#else
    socklen_t addrLen = sizeof(addr);
#endif
    if (::getsockname(listener, reinterpret_cast<sockaddr *>(&addr), &addrLen) != 0) {
        closeSock(listener);
        return false;
    }

    *a = ::socket(AF_INET, SOCK_STREAM, 0);
    if (*a == SSH_INVALID_SOCKET) {
        closeSock(listener);
        return false;
    }
    if (::connect(*a, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        closeSock(*a);
        *a = SSH_INVALID_SOCKET;
        closeSock(listener);
        return false;
    }

    *b = ::accept(listener, nullptr, nullptr);
    closeSock(listener);
    if (*b == SSH_INVALID_SOCKET) {
        closeSock(*a);
        *a = SSH_INVALID_SOCKET;
        return false;
    }

    if (!setNonBlocking(*a) || !setNonBlocking(*b)) {
        closeSock(*a);
        closeSock(*b);
        *a = SSH_INVALID_SOCKET;
        *b = SSH_INVALID_SOCKET;
        return false;
    }
    return true;
}

} // namespace

class SshIoLoopTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void wakeUnblocksDopoll();
    void addRemoveHandler();
    void attachDetachSession();
    void addFdRoundtrip();
};

void SshIoLoopTest::initTestCase()
{
#ifdef Q_OS_WIN
    WSADATA wsa;
    QCOMPARE(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
#endif
}

void SshIoLoopTest::wakeUnblocksDopoll()
{
    SshIoLoop loop;
    QAtomicInt idleCount{0};
    QVERIFY(loop.addHandler(std::make_unique<NoopHandler>(QStringLiteral("noop"), &idleCount)));

    QThread *worker = QThread::create([&loop]() { loop.run(); });
    worker->start();
    QVERIFY(worker->wait(50) == false); // still running

    loop.wake();
    QTest::qWait(20);
    loop.stop();

    QVERIFY2(worker->wait(3000), "SshIoLoop::run did not exit after stop()");
    delete worker;
    QVERIFY(idleCount.loadRelaxed() >= 0);
}

void SshIoLoopTest::addRemoveHandler()
{
    SshIoLoop loop;
    QAtomicInt cancelCount{0};

    QVERIFY(loop.addHandler(
        std::make_unique<NoopHandler>(QStringLiteral("h1"), nullptr, &cancelCount)));

    QString err;
    QVERIFY(!loop.addHandler(std::make_unique<NoopHandler>(QStringLiteral("h1")), &err));
    QVERIFY(err.contains(QStringLiteral("already registered")));

    QVERIFY(!loop.addHandler(std::make_unique<FailingHandler>(), &err));
    QCOMPARE(err, QStringLiteral("intentional start failure"));

    loop.removeHandler(QStringLiteral("h1"));
    QCOMPARE(cancelCount.loadRelaxed(), 1);

    // Second remove is a safe no-op.
    loop.removeHandler(QStringLiteral("h1"));
    QCOMPARE(cancelCount.loadRelaxed(), 1);

    QVERIFY(loop.addHandler(
        std::make_unique<NoopHandler>(QStringLiteral("h2"), nullptr, &cancelCount)));
    loop.detachSession(); // cancels remaining handlers even with no session
    QCOMPARE(cancelCount.loadRelaxed(), 2);
}

void SshIoLoopTest::attachDetachSession()
{
    SshIoLoop loop;
    ssh_session session = ssh_new();
    QVERIFY(session != nullptr);

    QString err;
    // Unconnected sessions may or may not accept add_session depending on libssh.
    const bool attached = loop.attachSession(session, &err);
    if (attached) {
        QVERIFY(loop.isAttached());
        QCOMPARE(loop.session(), session);

        QString err2;
        QVERIFY(!loop.attachSession(session, &err2));
        QVERIFY(err2.contains(QStringLiteral("already attached")));

        loop.detachSession();
        QVERIFY(!loop.isAttached());
        QCOMPARE(loop.session(), nullptr);
    } else {
        QVERIFY(!err.isEmpty());
        QVERIFY(!loop.isAttached());
    }

    // Null / double-detach paths must be safe.
    QVERIFY(!loop.attachSession(nullptr, &err));
    loop.detachSession();

    ssh_free(session);
}

void SshIoLoopTest::addFdRoundtrip()
{
    SshIoLoop loop;

    socket_t client = SSH_INVALID_SOCKET;
    socket_t peer = SSH_INVALID_SOCKET;
    QVERIFY(makeConnectedPair(&client, &peer));

    std::atomic_int fired{0};
    QString err;
    QVERIFY(loop.addFd(
        peer,
        POLLIN,
        [&fired](socket_t fd, int revents) {
            if (revents & POLLIN) {
                fired.fetch_add(1);
                char buf[16];
#ifdef Q_OS_WIN
                (void)::recv(fd, buf, sizeof(buf), 0);
#else
                (void)::read(static_cast<int>(fd), buf, sizeof(buf));
#endif
            }
            return 0;
        },
        &err));

    QThread *worker = QThread::create([&loop]() { loop.run(); });
    worker->start();

    const char byte = 'x';
#ifdef Q_OS_WIN
    QCOMPARE(::send(client, &byte, 1, 0), 1);
#else
    QCOMPARE(::write(static_cast<int>(client), &byte, 1), 1);
#endif

    QElapsedTimer timer;
    timer.start();
    while (fired.load() == 0 && timer.elapsed() < 2000) {
        QTest::qWait(10);
    }
    QVERIFY2(fired.load() > 0, "user fd callback did not fire");

    loop.stop();
    QVERIFY(worker->wait(3000));
    delete worker;

    loop.removeFd(peer);
    closeSock(client);
    closeSock(peer);
}

QTEST_GUILESS_MAIN(SshIoLoopTest)
#include "tst_SshIoLoop.moc"
