// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/fs/TransferJobStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class TransferJobStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void jobKeyIsStable();
    void saveLoadRemoveRoundTrip();
    void loadLatestAndRemoveAll();

private:
    QTemporaryDir m_temp;
    QString m_previousAppData;
};

void TransferJobStoreTest::initTestCase()
{
    QVERIFY(m_temp.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("EasySshTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TransferJobStoreTest"));
    QStandardPaths::setTestModeEnabled(true);
    // Keep local filepart/meta paths inside the temp tree.
    QVERIFY(QDir(m_temp.path()).exists());
}

void TransferJobStoreTest::cleanup()
{
    const QUuid connectionId = QUuid::createUuid();
    TransferJobStore::removeAllForConnection(connectionId);
}

void TransferJobStoreTest::jobKeyIsStable()
{
    const QString first = TransferJobStore::jobKey(
        TransferDirection::Upload, QStringLiteral("/local"), QStringLiteral("/remote"));
    const QString second = TransferJobStore::jobKey(
        TransferDirection::Upload, QStringLiteral("/local"), QStringLiteral("/remote"));
    const QString other = TransferJobStore::jobKey(
        TransferDirection::Download, QStringLiteral("/local"), QStringLiteral("/remote"));
    QCOMPARE(first, second);
    QVERIFY(first != other);
}

void TransferJobStoreTest::saveLoadRemoveRoundTrip()
{
    TransferJob job;
    job.connectionId = QUuid::createUuid();
    job.direction = TransferDirection::Download;
    job.localPath = m_temp.filePath(QStringLiteral("download.bin"));
    job.remoteFinalPath = QStringLiteral("/remote/download.bin");
    job.filepartPath = transferFilepartPathForFinal(job.localPath);
    job.bytesDone = 10;
    job.bytesTotal = 100;
    job.sourceSize = 100;
    job.updatedAtMs = 1000;
    job.backend = FsBackend::Sftp;
    job.lastReason = TransferEndReason::Interrupted;
    job.lastMessage = QStringLiteral("paused");

    QString error;
    QVERIFY(TransferJobStore::save(job, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const auto loaded = TransferJobStore::loadForPaths(
        job.connectionId, job.direction, job.localPath, job.remoteFinalPath);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->bytesDone, 10LL);
    QCOMPARE(loaded->lastMessage, QStringLiteral("paused"));

    QVERIFY(QFile::exists(transferMetaPathForFilepart(job.filepartPath)));

    QVERIFY(TransferJobStore::removeJob(*loaded, &error));
    QVERIFY(!TransferJobStore::load(
        job.connectionId,
        TransferJobStore::jobKey(job.direction, job.localPath, job.remoteFinalPath)));
    QVERIFY(!QFile::exists(transferMetaPathForFilepart(job.filepartPath)));
}

void TransferJobStoreTest::loadLatestAndRemoveAll()
{
    const QUuid connectionId = QUuid::createUuid();

    TransferJob older;
    older.connectionId = connectionId;
    older.direction = TransferDirection::Upload;
    older.localPath = m_temp.filePath(QStringLiteral("a.bin"));
    older.remoteFinalPath = QStringLiteral("/remote/a.bin");
    older.updatedAtMs = 100;

    TransferJob newer = older;
    newer.localPath = m_temp.filePath(QStringLiteral("b.bin"));
    newer.remoteFinalPath = QStringLiteral("/remote/b.bin");
    newer.updatedAtMs = 200;
    newer.lastMessage = QStringLiteral("latest");

    QVERIFY(TransferJobStore::save(older));
    QVERIFY(TransferJobStore::save(newer));

    const auto latest = TransferJobStore::loadLatest(connectionId);
    QVERIFY(latest.has_value());
    QCOMPARE(latest->lastMessage, QStringLiteral("latest"));
    QCOMPARE(latest->updatedAtMs, 200LL);

    TransferJobStore::removeAllForConnection(connectionId);
    QVERIFY(!TransferJobStore::loadLatest(connectionId).has_value());
}

QTEST_GUILESS_MAIN(TransferJobStoreTest)

#include "tst_TransferJobStore.moc"
