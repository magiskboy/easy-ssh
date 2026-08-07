// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/fs/ShellExecRunner.h"
#include "core/tunnel/TunnelHostIoHandler.h"

#include <QTest>

class ScpPhase5HelpersTest : public QObject
{
    Q_OBJECT

private slots:
    void wrapCommandEmptyShell();
    void wrapCommandWithShell();
    void tunnelHostHandlerId();
};

void ScpPhase5HelpersTest::wrapCommandEmptyShell()
{
    QCOMPARE(ShellExecRunner::wrapCommand({}, QStringLiteral("ls -la")), QStringLiteral("ls -la"));
}

void ScpPhase5HelpersTest::wrapCommandWithShell()
{
    const QString wrapped =
        ShellExecRunner::wrapCommand(QStringLiteral("/bin/bash"), QStringLiteral("pwd"));
    QVERIFY(wrapped.startsWith(QStringLiteral("/bin/bash -c ")));
    QVERIFY(wrapped.contains(QStringLiteral("pwd")));
}

void ScpPhase5HelpersTest::tunnelHostHandlerId()
{
    QCOMPARE(TunnelHostIoHandler::handlerId(), QStringLiteral("tunnel-host"));
    TunnelHostIoHandler handler({});
    QCOMPARE(handler.id(), TunnelHostIoHandler::handlerId());
}

QTEST_MAIN(ScpPhase5HelpersTest)
#include "tst_ScpPhase5Helpers.moc"
