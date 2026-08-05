/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/fs/ShellCommandSet.h"

#include <QtTest>

class ShellCommandSetTest final : public QObject
{
    Q_OBJECT

private slots:
    void shellQuoteAndTemplates();
    void startupCommands();
    void parseLsListingHandlesVariants();
    void parseLsSingleFindsEntry();
};

void ShellCommandSetTest::shellQuoteAndTemplates()
{
    QCOMPARE(ShellCommandSet::shellQuote(QStringLiteral("plain")), QStringLiteral("'plain'"));
    QCOMPARE(ShellCommandSet::shellQuote(QStringLiteral("it's")), QStringLiteral("'it'\\''s'"));

    ShellCommandSet commands;
    QCOMPARE(commands.formatMkdir(QStringLiteral("/tmp/a b")), QStringLiteral("mkdir '/tmp/a b'"));
    QCOMPARE(commands.formatRemove(QStringLiteral("/tmp/x")), QStringLiteral("rm -f -r '/tmp/x'"));
    QCOMPARE(commands.formatRename(QStringLiteral("/a"), QStringLiteral("/b")),
             QStringLiteral("mv -f '/a' '/b'"));
    QCOMPARE(commands.formatPwd(), QStringLiteral("pwd"));
    QCOMPARE(commands.formatRealpath(QStringLiteral("/tmp")), QStringLiteral("realpath -e '/tmp'"));
    QCOMPARE(commands.formatSymlink(QStringLiteral("../x"), QStringLiteral("/tmp/l")),
             QStringLiteral("ln -s '../x' '/tmp/l'"));
    QCOMPARE(commands.formatReadlink(QStringLiteral("/tmp/l")),
             QStringLiteral("readlink -n '/tmp/l'"));
    QCOMPARE(commands.formatTestDirectory(QStringLiteral("/tmp/a b")),
             QStringLiteral("test -d '/tmp/a b'"));
    QCOMPARE(commands.formatListDirectory(QStringLiteral("/home"), QStringLiteral("--full-time")),
             QStringLiteral("ls -la --full-time '/home'"));
    QCOMPARE(commands.formatListFile(QStringLiteral("/home/a"), QString()),
             QStringLiteral("ls -la -d '/home/a'"));

    ShellCommandSetConfig custom;
    custom.listFileCommand = QStringLiteral("stat %1");
    custom.mkdirCommand = QStringLiteral("install -d %1");
    custom.symlinkCommand = QStringLiteral("ln -snf %1 %2");
    custom.readlinkCommand = QStringLiteral("readlink %1");
    commands.setConfig(custom);
    QCOMPARE(commands.formatListFile(QStringLiteral("/x"), QString()), QStringLiteral("stat '/x'"));
    QCOMPARE(commands.formatMkdir(QStringLiteral("/y")), QStringLiteral("install -d '/y'"));
    QCOMPARE(commands.formatSymlink(QStringLiteral("t"), QStringLiteral("l")),
             QStringLiteral("ln -snf 't' 'l'"));
    QCOMPARE(commands.formatReadlink(QStringLiteral("/l")), QStringLiteral("readlink '/l'"));
}

void ShellCommandSetTest::startupCommands()
{
    ShellCommandSet defaults;
    const QString startup = defaults.formatStartupCommands();
    QVERIFY(startup.contains(QStringLiteral("unalias ls")));
    QVERIFY(startup.contains(QStringLiteral("unalias readlink")));
    QVERIFY(startup.contains(QStringLiteral("unalias ln")));
    QVERIFY(startup.contains(QStringLiteral("unalias test")));
    QVERIFY(startup.contains(QStringLiteral("unset LANG")));

    ShellCommandSetConfig cleared;
    cleared.clearAliases = false;
    cleared.clearNationalVars = false;
    defaults.setConfig(cleared);
    QVERIFY(defaults.formatStartupCommands().isEmpty());
}

void ShellCommandSetTest::parseLsListingHandlesVariants()
{
    const QString listing = QStringLiteral(
        "total 8\n"
        "\x1B[0mdrwxr-xr-x 2 alice alice 4096 Jan  2 10:11 my dir\x1B[0m\n"
        "-rw-r--r-- 1 alice alice  128 2026-01-02 10:11:12.123456789 +0000 note.txt\n"
        "lrwxrwxrwx 1 alice alice    4 Jan  2 10:11 link -> dest\n"
        "lrwxrwxrwx 1 alice alice    5 Jan  2 10:11 rel -> ../x\n"
        "bad line\n"
        "drwxr-xr-x 2 alice alice 4096 Jan  2 2025 archive\n");

    QVector<RemoteEntry> entries;
    QString error;
    QVERIFY(ShellCommandSet::parseLsListing(listing, &entries, QStringLiteral("/home"), &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(entries.size(), 5);

    QCOMPARE(entries.at(0).name, QStringLiteral("my dir"));
    QVERIFY(entries.at(0).isDir);
    QVERIFY(!entries.at(0).isSymlink);
    QCOMPARE(entries.at(0).path, QStringLiteral("/home/my dir"));

    QCOMPARE(entries.at(1).name, QStringLiteral("note.txt"));
    QVERIFY(entries.at(1).mtime > 0);
    QVERIFY(!entries.at(1).isSymlink);

    QCOMPARE(entries.at(2).name, QStringLiteral("link"));
    QVERIFY(entries.at(2).isSymlink);
    QVERIFY(!entries.at(2).isDir);
    QCOMPARE(entries.at(2).linkTarget, QStringLiteral("dest"));
    QCOMPARE(entries.at(2).path, QStringLiteral("/home/link"));

    QCOMPARE(entries.at(3).name, QStringLiteral("rel"));
    QVERIFY(entries.at(3).isSymlink);
    QVERIFY(!entries.at(3).isDir);
    QCOMPARE(entries.at(3).linkTarget, QStringLiteral("../x"));

    QCOMPARE(entries.at(4).name, QStringLiteral("archive"));
    QVERIFY(entries.at(4).isDir);
    QVERIFY(!entries.at(4).isSymlink);

    QVERIFY(!ShellCommandSet::parseLsListing(QString(), nullptr, QString(), &error));
    QVERIFY(error.contains(QStringLiteral("missing entry buffer")));
}

void ShellCommandSetTest::parseLsSingleFindsEntry()
{
    const QString listing = QStringLiteral("-rw-r--r-- 1 alice alice 12 Jan  2 10:11 target.txt\n");
    RemoteEntry entry;
    QString error;
    QVERIFY(
        ShellCommandSet::parseLsSingle(listing, &entry, QStringLiteral("/tmp/target.txt"), &error));
    QCOMPARE(entry.name, QStringLiteral("target.txt"));
    QCOMPARE(entry.path, QStringLiteral("/tmp/target.txt"));
    QCOMPARE(entry.size, 12LL);

    QVERIFY(!ShellCommandSet::parseLsSingle(
        QStringLiteral("total 0\n"), &entry, QStringLiteral("/tmp/missing"), &error));
    QVERIFY(error.contains(QStringLiteral("Cannot parse")));
}

QTEST_GUILESS_MAIN(ShellCommandSetTest)

#include "tst_ShellCommandSet.moc"
