// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/explorer/process/ProcessParser.h"

#include <QtTest>

class ProcessParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void formatHelpers();
    void classifyFailure();
    void listCommand();
    void formatUserAndStartedDisplay();
    void parsePsListRejectsNullOutput();
    void parsePsListParsesValidRows();
};

void ProcessParserTest::formatHelpers()
{
    QCOMPARE(ProcessParser::formatStateDisplay("R"), QString("Running"));
    QCOMPARE(ProcessParser::formatStateDisplay("Ssl"), QString("Sleeping"));
    QCOMPARE(ProcessParser::formatStateDisplay("Q"), QString("Q"));
    QCOMPARE(ProcessParser::formatPriorityDisplay(-15), QString("Very high (-15)"));
    QCOMPARE(ProcessParser::formatPriorityDisplay(0), QString("Normal (0)"));
    QCOMPARE(ProcessParser::formatPriorityDisplay(11), QString("Very low (11)"));
    QCOMPARE(ProcessParser::formatMemoryFromKiB(-1), QString("\u2014"));
    QCOMPARE(ProcessParser::formatMemoryFromKiB(1536), QString("1.5 MiB"));

    ProcessInfo named;
    named.comm = QString("sshd");
    QCOMPARE(ProcessParser::displayName(named), QString("sshd"));

    ProcessInfo fromCommand;
    fromCommand.command = QString("/usr/bin/python3 app.py");
    QCOMPARE(ProcessParser::displayName(fromCommand), QString("python3"));

    ProcessInfo fallback;
    fallback.pid = 42;
    QCOMPARE(ProcessParser::displayName(fallback), QString("42"));
}

void ProcessParserTest::classifyFailure()
{
    QString message;

    QCOMPARE(ProcessParser::classifyFailure(
                 1, QByteArray("sh: ps: command not found"), QString(), &message),
             ExplorerCapability::Unavailable);
    QVERIFY(message.contains("ps is not available"));

    QCOMPARE(ProcessParser::classifyFailure(126, QByteArray(), QString(), &message),
             ExplorerCapability::PermissionDenied);
    QVERIFY(message.contains("Permission denied"));

    QCOMPARE(ProcessParser::classifyFailure(2, QByteArray("bad output"), QString(), &message),
             ExplorerCapability::Error);
    QCOMPARE(message, QString("bad output"));
}

void ProcessParserTest::listCommand()
{
    const QString command = ProcessParser::listCommand();
    QVERIFY(command.startsWith(QStringLiteral("ps -eo ")));
    QVERIFY(command.contains(QStringLiteral("pid=")));
    QVERIFY(command.contains(QStringLiteral("args=")));
}

void ProcessParserTest::formatUserAndStartedDisplay()
{
    ProcessInfo both;
    both.user = QStringLiteral("alice");
    both.uid = 1000;
    QCOMPARE(ProcessParser::formatUserDisplay(both), QStringLiteral("alice (1000)"));

    ProcessInfo userOnly;
    userOnly.user = QStringLiteral("bob");
    userOnly.uid = -1;
    QCOMPARE(ProcessParser::formatUserDisplay(userOnly), QStringLiteral("bob"));

    ProcessInfo uidOnly;
    uidOnly.uid = 42;
    QCOMPARE(ProcessParser::formatUserDisplay(uidOnly), QStringLiteral("42"));

    ProcessInfo empty;
    QCOMPARE(ProcessParser::formatUserDisplay(empty), QStringLiteral("\u2014"));

    // Pin wall clock so Today/Yesterday do not depend on CI run time.
    const QDateTime now(QDate(2026, 8, 8), QTime(12, 0, 0));
    QCOMPARE(ProcessParser::formatStartedDisplay(-1, now), QStringLiteral("\u2014"));
    QVERIFY(ProcessParser::formatStartedDisplay(30, now).contains(QStringLiteral("Today")));
    // 90000s = 25h → 2026-08-07 11:00 with the pinned noon clock.
    QVERIFY(ProcessParser::formatStartedDisplay(90000, now).contains(QStringLiteral("Yesterday")));
}

void ProcessParserTest::parsePsListRejectsNullOutput()
{
    QString error;
    QVERIFY(!ProcessParser::parsePsList(QByteArray(), nullptr, &error));
    QVERIFY(error.contains("Output buffer is null"));
}

void ProcessParserTest::parsePsListParsesValidRows()
{
    const QByteArray payload =
        "123 1 1000 alice 1.5 0.4 S 0 20 75 00:00:03 2048 4096 sshd /usr/sbin/sshd -D\n"
        "bad row\n"
        "456 1 0 root 9.9 1.1 R -5 10 5 00:00:01 1024 2048 python /usr/bin/python app.py\n";

    QVector<ProcessInfo> processes;
    QString error;
    QVERIFY(ProcessParser::parsePsList(payload, &processes, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(processes.size(), 2);

    const ProcessInfo first = processes.at(0);
    QCOMPARE(first.pid, 123LL);
    QCOMPARE(first.uid, 1000LL);
    QCOMPARE(first.user, QString("alice"));
    QCOMPARE(first.stateCode, QString("S"));
    QCOMPARE(first.rssKiB, 2048LL);
    QCOMPARE(first.command, QString("/usr/sbin/sshd -D"));

    const ProcessInfo second = processes.at(1);
    QCOMPARE(second.pid, 456LL);
    QCOMPARE(second.nice, -5);
    QCOMPARE(second.comm, QString("python"));
    QCOMPARE(second.command, QString("/usr/bin/python app.py"));
}

QTEST_GUILESS_MAIN(ProcessParserTest)

#include "tst_ProcessParser.moc"
