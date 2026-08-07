// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/explorer/service/ServiceParser.h"

#include <QtTest>

class ServiceParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void formatHelpers();
    void classifyFailure();
    void listAndInspectCommands();
    void parseListParsesJsonArrayAndNdjson();
    void parseInspectParsesJsonAndKeyValue();
};

void ServiceParserTest::formatHelpers()
{
    QCOMPARE(ServiceParser::normalizeActiveState(" active "), QString("active"));
    QCOMPARE(ServiceParser::normalizeActiveState("maintenance"), QString("maintenance"));
    QCOMPARE(ServiceParser::normalizeActiveState("mystery"), QString("unknown"));
    QCOMPARE(ServiceParser::formatActiveStateDisplay("reloading"), QString("Reloading"));
    QCOMPARE(ServiceParser::formatActiveStateDisplay("unknown"), QString("Unknown"));
    QCOMPARE(ServiceParser::formatOrDash("   "), QString("\u2014"));
    QCOMPARE(ServiceParser::formatPidDisplay(0), QString("\u2014"));
    QCOMPARE(ServiceParser::formatPidDisplay(1234), QString("1234"));
}

void ServiceParserTest::classifyFailure()
{
    QString message;

    QCOMPARE(
        ServiceParser::classifyFailure(127, QByteArray("no service manager"), QString(), &message),
        ExplorerCapability::Unavailable);
    QVERIFY(message.contains("No supported service manager"));

    QCOMPARE(ServiceParser::classifyFailure(
                 1, QByteArray("interactive authentication required"), QString(), &message),
             ExplorerCapability::PermissionDenied);
    QVERIFY(message.contains("Permission denied"));

    QCOMPARE(ServiceParser::classifyFailure(2, QByteArray(), QString("transport failed"), &message),
             ExplorerCapability::Error);
    QCOMPARE(message, QString("transport failed"));
}

void ServiceParserTest::listAndInspectCommands()
{
    const QString list = ServiceParser::listCommand();
    QVERIFY(list.contains(QStringLiteral("systemctl")));
    QVERIFY(list.contains(QStringLiteral("no service manager")));

    ServiceInfo empty;
    QCOMPARE(ServiceParser::inspectCommand(empty), QString());
    QCOMPARE(ServiceParser::followLogsCommand(empty), QString());

    ServiceInfo openrc;
    openrc.manager = QStringLiteral("openrc");
    openrc.unit = QStringLiteral("sshd");
    QCOMPARE(ServiceParser::inspectCommand(openrc), QString());
    QCOMPARE(ServiceParser::followLogsCommand(openrc), QString());

    ServiceInfo systemd;
    systemd.manager = QStringLiteral("systemd");
    systemd.unit = QStringLiteral("sshd.service");
    const QString inspect = ServiceParser::inspectCommand(systemd);
    QVERIFY(inspect.contains(QStringLiteral("systemctl show")));
    QVERIFY(inspect.contains(QStringLiteral("'sshd.service'")));
    QCOMPARE(ServiceParser::followLogsCommand(systemd, 0),
             QStringLiteral("journalctl --no-pager -f -n 1 -u 'sshd.service'"));
}

void ServiceParserTest::parseListParsesJsonArrayAndNdjson()
{
    const QByteArray jsonInventory = R"JSON(
[
  {"unit":"sshd.service","description":"OpenSSH server","load":"loaded","active":"active","sub":"running","main_pid":"123"},
  {"unit":"nginx.service","description":"Nginx","load":"loaded","active":"failed","sub":"failed","main_pid":456}
]
__EASY_SSH_UNIT_FILES__
[
  {"unit_file":"sshd.service","state":"enabled"},
  {"unit_file":"nginx.service","state":"disabled"}
]
)JSON";

    QVector<ServiceInfo> services;
    QString error;
    QVERIFY(ServiceParser::parseList(jsonInventory, &services, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(services.size(), 2);
    QCOMPARE(services.at(0).manager, QString("systemd"));
    QCOMPARE(services.at(0).unit, QString("sshd.service"));
    QCOMPARE(services.at(0).activeState, QString("active"));
    QCOMPARE(services.at(0).unitFileState, QString("enabled"));
    QCOMPARE(services.at(1).mainPid, 456LL);

    const QByteArray ndjsonInventory =
        "{\"manager\":\"systemd\",\"unit\":\"dbus.service\",\"description\":\"D-Bus\",\"load_"
        "state\":"
        "\"loaded\",\"active_state\":\"inactive\",\"sub_state\":\"dead\",\"unit_file_state\":"
        "\"static\",\"main_pid\":0}\n"
        "not-json\n";

    services.clear();
    QVERIFY(ServiceParser::parseList(ndjsonInventory, &services, &error));
    QCOMPARE(services.size(), 1);
    QCOMPARE(services.first().unit, QString("dbus.service"));
    QCOMPARE(services.first().activeState, QString("inactive"));
}

void ServiceParserTest::parseInspectParsesJsonAndKeyValue()
{
    const ServiceInfo seed{QString("systemd"),
                           QString("sshd.service"),
                           QString("OpenSSH server"),
                           QString("loaded"),
                           QString("active"),
                           QString("running"),
                           QString("enabled"),
                           999};

    const QByteArray jsonPayload = R"JSON(
{
  "unit":"sshd.service",
  "description":"OpenSSH Daemon",
  "load":"loaded",
  "active":"active",
  "sub":"running",
  "main_pid":"123",
  "FragmentPath":"/usr/lib/systemd/system/sshd.service",
  "ActiveEnterTimestamp":"Mon 2026-01-01 00:00:00 UTC",
  "ExecMainStartTimestamp":"Mon 2026-01-01 00:00:01 UTC",
  "Type":"notify",
  "Restart":"on-failure",
  "RemainAfterExit":"yes"
}
)JSON";

    ServiceInspectInfo inspect;
    QString error;
    QVERIFY(ServiceParser::parseInspect(jsonPayload, seed, &inspect, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(inspect.base.unit, QString("sshd.service"));
    QCOMPARE(inspect.base.mainPid, 123LL);
    QCOMPARE(inspect.fragmentPath, QString("/usr/lib/systemd/system/sshd.service"));
    QCOMPARE(inspect.type, QString("notify"));
    QVERIFY(inspect.remainAfterExit);

    const QByteArray kvPayload = "Id=cron.service\n"
                                 "Description=Cron Daemon\n"
                                 "LoadState=loaded\n"
                                 "ActiveState=inactive\n"
                                 "SubState=dead\n"
                                 "UnitFileState=enabled\n"
                                 "MainPID=0\n"
                                 "FragmentPath=/usr/lib/systemd/system/cron.service\n"
                                 "Type=simple\n"
                                 "Restart=no\n"
                                 "RemainAfterExit=no\n";

    QVERIFY(ServiceParser::parseInspect(kvPayload, seed, &inspect, &error));
    QCOMPARE(inspect.base.unit, QString("cron.service"));
    QCOMPARE(inspect.base.activeState, QString("inactive"));
    QCOMPARE(inspect.fragmentPath, QString("/usr/lib/systemd/system/cron.service"));
    QVERIFY(!inspect.remainAfterExit);
}

QTEST_GUILESS_MAIN(ServiceParserTest)

#include "tst_ServiceParser.moc"
