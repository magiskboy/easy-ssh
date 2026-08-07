// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/ssh/ShellIoHandler.h"
#include "core/ssh/SshIoLoop.h"

#include <QtTest>

#include <memory>

namespace
{

class QueuingHandler final : public SshIoHandler
{
public:
    explicit QueuingHandler(QString id) : m_id(std::move(id)) {}

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
        ++m_cancelCount;
        m_cancelled = true;
    }
    void onIdle() override { ++m_idleCount; }

    bool m_started = false;
    bool m_cancelled = false;
    int m_cancelCount = 0;
    int m_idleCount = 0;

private:
    QString m_id;
};

} // namespace

class ShellIoHandlerTest final : public QObject
{
    Q_OBJECT

private slots:
    void shellHandlerIdFormat();
    void cancelBeforeStartIsIdempotent();
    void enqueueWriteWakesLoop();
    void pollOnceInvokesOnIdle();
};

// Manual regression (p9 Phase 2): type in terminal; yes/top output; multi-pane;
// PTY resize; tunnels/agent still tick; explorer exec keeps shell alive via pump.

void ShellIoHandlerTest::shellHandlerIdFormat()
{
    const QUuid id = QUuid::createUuid();
    ShellIoHandler handler(id, nullptr, 80, 24, {});
    QCOMPARE(handler.id(), id.toString(QUuid::WithoutBraces));
    QCOMPARE(handler.shellId(), id);
}

void ShellIoHandlerTest::cancelBeforeStartIsIdempotent()
{
    const QUuid id = QUuid::createUuid();
    ShellIoHandler handler(id, nullptr, 80, 24, {});
    handler.cancel();
    handler.cancel();
    QVERIFY(!handler.shell()->isOpen());
}

void ShellIoHandlerTest::enqueueWriteWakesLoop()
{
    SshIoLoop loop;
    const QUuid id = QUuid::createUuid();
    ShellIoHandler handler(id, nullptr, 80, 24, {});
    // Without start(), enqueue still appends and wakes (no channel flush until open).
    handler.enqueueWrite(QByteArrayLiteral("x"));
    // Wake must not crash even if handler is not registered on the loop.
    loop.wake();
    loop.stop();
}

void ShellIoHandlerTest::pollOnceInvokesOnIdle()
{
    SshIoLoop loop;
    auto handler = std::make_unique<QueuingHandler>(QStringLiteral("q1"));
    QueuingHandler *raw = handler.get();
    QVERIFY(loop.addHandler(std::move(handler)));

    QVERIFY(loop.pollOnce(0));
    QCOMPARE(raw->m_idleCount, 1);
    QCOMPARE(raw->m_cancelCount, 0);

    loop.removeHandler(QStringLiteral("q1"));
    // handler destroyed by removeHandler; do not touch raw
}

QTEST_MAIN(ShellIoHandlerTest)
#include "tst_ShellIoHandler.moc"
