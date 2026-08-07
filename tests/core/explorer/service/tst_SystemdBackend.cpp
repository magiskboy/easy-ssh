// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/explorer/service/backends/SystemdBackend.h"

#include <QtTest>

class SystemdBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void managerIdAndSnippet();
    void parseLineAcceptsAlternateKeys();
    void parseJsonInventoryMergesUnitFiles();
    void parseJsonInventoryRejectsInvalidInput();
};

void SystemdBackendTest::managerIdAndSnippet()
{
    const SystemdBackend systemd;
    QCOMPARE(systemd.managerId(), QStringLiteral("systemd"));
    const QString snippet = systemd.remoteListSnippet();
    QVERIFY(snippet.contains(QStringLiteral("systemctl list-units")));
    QVERIFY(snippet.contains(QStringLiteral("__EASY_SSH_UNIT_FILES__")));
}

void SystemdBackendTest::parseLineAcceptsAlternateKeys()
{
    const SystemdBackend systemd;
    ServiceInfo info;
    QVERIFY(!systemd.parseLine(QJsonObject{}, &info));

    QJsonObject object{{QStringLiteral("manager"), QStringLiteral("systemd")},
                       {QStringLiteral("unit"), QStringLiteral("sshd.service")},
                       {QStringLiteral("description"), QStringLiteral("OpenSSH")},
                       {QStringLiteral("load"), QStringLiteral("loaded")},
                       {QStringLiteral("active"), QStringLiteral("active")},
                       {QStringLiteral("sub"), QStringLiteral("running")},
                       {QStringLiteral("state"), QStringLiteral("enabled")},
                       {QStringLiteral("main_pid"), QStringLiteral("123")}};
    QVERIFY(systemd.parseLine(object, &info));
    QCOMPARE(info.unit, QStringLiteral("sshd.service"));
    QCOMPARE(info.loadState, QStringLiteral("loaded"));
    QCOMPARE(info.activeState, QStringLiteral("active"));
    QCOMPARE(info.subState, QStringLiteral("running"));
    QCOMPARE(info.unitFileState, QStringLiteral("enabled"));
    QCOMPARE(info.mainPid, 123LL);
}

void SystemdBackendTest::parseJsonInventoryMergesUnitFiles()
{
    const SystemdBackend systemd;
    const QByteArray payload = R"JSON(
[
  {"unit":"sshd.service","description":"OpenSSH","load":"loaded","active":"active","sub":"running","main_pid":"10"},
  {"unit":"cron.service","description":"Cron","load":"loaded","active":"inactive","sub":"dead","main_pid":0}
]
__EASY_SSH_UNIT_FILES__
[
  {"unit_file":"sshd.service","state":"enabled"},
  {"unit_file":"cron.service","state":"disabled"}
]
)JSON";

    QVector<ServiceInfo> services;
    QString error;
    QVERIFY(systemd.parseJsonInventory(payload, &services, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(services.size(), 2);
    QCOMPARE(services.at(0).unitFileState, QStringLiteral("enabled"));
    QCOMPARE(services.at(1).unitFileState, QStringLiteral("disabled"));
    QCOMPARE(services.at(0).mainPid, 10LL);
}

void SystemdBackendTest::parseJsonInventoryRejectsInvalidInput()
{
    const SystemdBackend systemd;
    QString error;
    QVERIFY(!systemd.parseJsonInventory(QByteArrayLiteral("not-json"), nullptr, &error));
    QVERIFY(error.contains(QStringLiteral("null")));

    QVector<ServiceInfo> services;
    QVERIFY(!systemd.parseJsonInventory(QByteArrayLiteral("{oops}"), &services, &error));
    QVERIFY(error.contains(QStringLiteral("Failed to parse")));
}

QTEST_GUILESS_MAIN(SystemdBackendTest)

#include "tst_SystemdBackend.moc"
