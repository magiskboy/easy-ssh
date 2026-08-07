// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/ssh/ExecIoHandler.h"
#include "core/ssh/SshIoLoop.h"

#include <QtTest>

#include <memory>

class ExecIoHandlerTest final : public QObject
{
    Q_OBJECT

private slots:
    void handlerIdIsUuid();
    void cancelBeforeStartIsIdempotent();
    void startFailsWithoutSession();
};

// Manual regression (p9 Phase 4): Process + Container + terminal; hide Process tab
// (no remote poll); inspect dialog; disconnect mid-poll.

void ExecIoHandlerTest::handlerIdIsUuid()
{
    ExecIoHandler handler(nullptr, QStringLiteral("req-1"), QStringLiteral("true"), {});
    QVERIFY(!handler.id().isEmpty());
    QCOMPARE(handler.id().size(), 36); // UUID WithoutBraces
    QCOMPARE(handler.requestId(), QStringLiteral("req-1"));
}

void ExecIoHandlerTest::cancelBeforeStartIsIdempotent()
{
    int completed = 0;
    int finished = 0;
    ExecIoHandler::Hooks hooks;
    hooks.completed = [&]() { ++completed; };
    hooks.finished =
        [&](const QString &, int, const QByteArray &, const QByteArray &, const QString &) {
            ++finished;
        };
    ExecIoHandler handler(nullptr, QStringLiteral("req"), QStringLiteral("true"), hooks);
    handler.cancel();
    handler.cancel();
    // Not started → cancel must not invoke completed/finished.
    QCOMPARE(completed, 0);
    QCOMPARE(finished, 0);
}

void ExecIoHandlerTest::startFailsWithoutSession()
{
    SshIoLoop loop;
    int completed = 0;
    ExecIoHandler::Hooks hooks;
    auto handler = std::make_unique<ExecIoHandler>(
        nullptr, QStringLiteral("req"), QStringLiteral("true"), hooks);
    const QString id = handler->id();
    handler->setCompletedHook([&]() { ++completed; });
    QString error;
    QVERIFY(!loop.addHandler(std::move(handler), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(completed, 0);
    QCOMPARE(loop.handler(id), nullptr);
}

QTEST_MAIN(ExecIoHandlerTest)
#include "tst_ExecIoHandler.moc"
