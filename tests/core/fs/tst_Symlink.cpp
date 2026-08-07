// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/fs/Symlink.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class SymlinkTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolveRelativeAndAbsolute();
    void directoryListPath();
    void splitLsName();
    void normalizeTarget();
    void isDirectoryLike();
    void createAndReadLocal();
};

void SymlinkTest::resolveRelativeAndAbsolute()
{
    QCOMPARE(Symlink::resolve(
                 {.linkPath = QStringLiteral("/home/a/link"), .target = QStringLiteral("/abs/x")}),
             QStringLiteral("/abs/x"));
    QCOMPARE(Symlink::resolve(
                 {.linkPath = QStringLiteral("/home/a/link"), .target = QStringLiteral("dest")}),
             QStringLiteral("/home/a/dest"));
    QCOMPARE(Symlink::resolve(
                 {.linkPath = QStringLiteral("/home/a/link"), .target = QStringLiteral("../b")}),
             QStringLiteral("/home/b"));
    QCOMPARE(Symlink::resolve({.linkPath = QStringLiteral("/link"), .target = QStringLiteral("x")}),
             QStringLiteral("/x"));
    QCOMPARE(Symlink::resolve({.linkPath = QStringLiteral("link"), .target = QStringLiteral("x")}),
             QStringLiteral("x"));
    QVERIFY(Symlink::resolve({.linkPath = QStringLiteral("/a/l"), .target = {}}).isEmpty());
}

void SymlinkTest::directoryListPath()
{
    QCOMPARE(Symlink::directoryListPath(QStringLiteral("/")), QStringLiteral("/"));
    QCOMPARE(Symlink::directoryListPath(QString()), QStringLiteral("/"));
    QCOMPARE(Symlink::directoryListPath(QStringLiteral("/tmp")), QStringLiteral("/tmp/"));
    QCOMPARE(Symlink::directoryListPath(QStringLiteral("/tmp/")), QStringLiteral("/tmp/"));
}

void SymlinkTest::splitLsName()
{
    {
        const Symlink::LsNameParts parts = Symlink::splitLsName(QStringLiteral("link -> dest"));
        QCOMPARE(parts.name, QStringLiteral("link"));
        QCOMPARE(parts.target, QStringLiteral("dest"));
    }
    {
        const Symlink::LsNameParts parts = Symlink::splitLsName(QStringLiteral("plain name"));
        QCOMPARE(parts.name, QStringLiteral("plain name"));
        QVERIFY(parts.target.isEmpty());
    }
    {
        const Symlink::LsNameParts parts = Symlink::splitLsName(QStringLiteral("rel -> ../x"));
        QCOMPARE(parts.name, QStringLiteral("rel"));
        QCOMPARE(parts.target, QStringLiteral("../x"));
    }
}

void SymlinkTest::normalizeTarget()
{
    QCOMPARE(Symlink::normalizeTarget(QStringLiteral("dest\r\r")), QStringLiteral("dest"));
    QCOMPARE(Symlink::normalizeTarget(QStringLiteral("dest")), QStringLiteral("dest"));
}

void SymlinkTest::isDirectoryLike()
{
    RemoteEntry file;
    file.isDir = false;
    file.isSymlink = false;
    QVERIFY(!Symlink::isDirectoryLike(file));

    RemoteEntry dir;
    dir.isDir = true;
    QVERIFY(Symlink::isDirectoryLike(dir));

    RemoteEntry linkFile;
    linkFile.isSymlink = true;
    linkFile.linkIsDir = false;
    QVERIFY(!Symlink::isDirectoryLike(linkFile));

    RemoteEntry linkDir;
    linkDir.isSymlink = true;
    linkDir.linkIsDir = true;
    QVERIFY(Symlink::isDirectoryLike(linkDir));
}

void SymlinkTest::createAndReadLocal()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString linkPath = QDir(tmp.path()).filePath(QStringLiteral("sub/link"));
    QString error;
    QVERIFY(Symlink::create({.linkPath = linkPath, .target = QStringLiteral("../target")}, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QString target;
    QVERIFY(Symlink::read(linkPath, target, &error));
    QCOMPARE(target, QStringLiteral("../target"));

    // Replace existing symlink.
    QVERIFY(Symlink::create({.linkPath = linkPath, .target = QStringLiteral("/other")}, &error));
    QVERIFY(Symlink::read(linkPath, target, &error));
    QCOMPARE(target, QStringLiteral("/other"));

    QVERIFY(!Symlink::read(QDir(tmp.path()).filePath(QStringLiteral("missing")), target, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(SymlinkTest)
#include "tst_Symlink.moc"
