/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/explorer/container/ContainerParser.h"

#include <QtTest>

class ContainerParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void formatHelpers();
    void classifyFailure();
    void listAndInspectCommands();
    void formatStateDisplay();
    void parseListParsesInventoryAndStats();
    void parseInspectParsesDockerPayloadAndStatsTrailer();
};

void ContainerParserTest::formatHelpers()
{
    QCOMPARE(ContainerParser::normalizeState("Up 10 minutes"), QString("running"));
    QCOMPARE(ContainerParser::normalizeState("removing"), QString("exited"));
    QCOMPARE(ContainerParser::normalizeState("paused"), QString("paused"));
    QCOMPARE(ContainerParser::normalizeState(""), QString("unknown"));
    QVERIFY(qAbs(ContainerParser::parsePercentValue("12.34%") - 12.34) < 0.0001);
    QCOMPARE(ContainerParser::parsePercentValue("N/A"), -1.0);
    QCOMPARE(ContainerParser::formatCpuDisplay(9.876), QString("9.88"));
    QCOMPARE(ContainerParser::formatCpuDisplay(12.34), QString("12.3"));
    QCOMPARE(ContainerParser::formatMemPercentDisplay(-1.0), QString("\u2014"));
    QCOMPARE(ContainerParser::shortId("1234567890abcdef"), QString("1234567890ab"));
    QCOMPARE(ContainerParser::formatOrDash(" "), QString("\u2014"));
    QCOMPARE(ContainerParser::joinElidable({QString(" a "), QString(), QString("b")},
                                           QStringLiteral(", ")),
             QString("a, b"));

    ContainerInfo info;
    info.containerId = QString("1234567890abcdef");
    QCOMPARE(ContainerParser::displayName(info), QString("1234567890ab"));
}

void ContainerParserTest::classifyFailure()
{
    QString message;

    QCOMPARE(ContainerParser::classifyFailure(
                 127, QByteArray("no container runtime"), QString(), &message),
             ExplorerCapability::Unavailable);
    QVERIFY(message.contains("No supported container runtime"));

    QCOMPARE(ContainerParser::classifyFailure(
                 1, QByteArray("cannot connect to the docker daemon"), QString(), &message),
             ExplorerCapability::PermissionDenied);
    QVERIFY(message.contains("Permission denied"));

    QCOMPARE(
        ContainerParser::classifyFailure(2, QByteArray(), QString("transport failed"), &message),
        ExplorerCapability::Error);
    QCOMPARE(message, QString("transport failed"));
}

void ContainerParserTest::listAndInspectCommands()
{
    const QString list = ContainerParser::listCommand();
    QVERIFY(list.contains(QStringLiteral("podman")));
    QVERIFY(list.contains(QStringLiteral("docker")));
    QVERIFY(list.contains(QStringLiteral("ctr")));
    QVERIFY(list.contains(QStringLiteral("no container runtime")));

    ContainerInfo empty;
    QCOMPARE(ContainerParser::inspectCommand(empty), QString());

    ContainerInfo docker;
    docker.runtime = QStringLiteral("docker");
    docker.containerId = QStringLiteral("abc123");
    QVERIFY(ContainerParser::inspectCommand(docker).contains(QStringLiteral("docker inspect")));

    ContainerInfo podman;
    podman.runtime = QStringLiteral("podman");
    podman.containerId = QStringLiteral("def456");
    QVERIFY(ContainerParser::inspectCommand(podman).contains(
        QStringLiteral("podman container inspect")));

    ContainerInfo ctr;
    ctr.runtime = QStringLiteral("containerd");
    ctr.containerId = QStringLiteral("789abc");
    ctr.runtimeNamespace = QStringLiteral("k8s.io");
    const QString ctrCmd = ContainerParser::inspectCommand(ctr);
    QVERIFY(ctrCmd.contains(QStringLiteral("ctr -n 'k8s.io'")));
    QVERIFY(ctrCmd.contains(QStringLiteral("'789abc'")));
}

void ContainerParserTest::formatStateDisplay()
{
    QCOMPARE(ContainerParser::formatStateDisplay(QStringLiteral("running")),
             QStringLiteral("Running"));
    QCOMPARE(ContainerParser::formatStateDisplay(QStringLiteral("exited")),
             QStringLiteral("Exited"));
    QCOMPARE(ContainerParser::formatStateDisplay(QStringLiteral("created")),
             QStringLiteral("Created"));
    QCOMPARE(ContainerParser::formatStateDisplay(QStringLiteral("paused")),
             QStringLiteral("Paused"));
    QCOMPARE(ContainerParser::formatStateDisplay(QStringLiteral("weird")),
             QStringLiteral("Unknown"));
}

void ContainerParserTest::parseListParsesInventoryAndStats()
{
    const QByteArray payload =
        "{\"runtime\":\"docker\",\"id\":\"1234567890abcdef1234\",\"name\":\"/web\",\"image\":"
        "\"nginx:latest\",\"state\":\"Up 3 minutes\",\"pid\":0,\"namespace\":\"\"}\n"
        "{\"runtime\":\"docker\",\"kind\":\"stats\",\"id\":\"1234567890ab\",\"cpu\":\"12.5%\","
        "\"mem_percent\":\"25.2%\",\"mem_usage\":\"10MiB / 100MiB\"}\n"
        "{\"runtime\":\"containerd\",\"id\":\"abcdef1234567890\",\"name\":\"\",\"image\":\"redis\","
        "\"state\":\"created\",\"pid\":321,\"namespace\":\"k8s.io\"}\n";

    QVector<ContainerInfo> containers;
    QString error;
    QVERIFY(ContainerParser::parseList(payload, &containers, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(containers.size(), 2);

    const ContainerInfo docker = containers.at(0);
    QCOMPARE(docker.runtime, QString("docker"));
    QCOMPARE(docker.name, QString("web"));
    QCOMPARE(docker.state, QString("running"));
    QVERIFY(qAbs(docker.cpuPercent - 12.5) < 0.0001);
    QVERIFY(qAbs(docker.memPercent - 25.2) < 0.0001);
    QCOMPARE(docker.memUsage, QString("10MiB / 100MiB"));

    const ContainerInfo ctr = containers.at(1);
    QCOMPARE(ctr.runtime, QString("containerd"));
    QCOMPARE(ctr.name, QString("abcdef123456"));
    QCOMPARE(ctr.runtimeNamespace, QString("k8s.io"));
    QCOMPARE(ctr.pid, 321LL);
}

void ContainerParserTest::parseInspectParsesDockerPayloadAndStatsTrailer()
{
    ContainerInfo seed;
    seed.runtime = QString("docker");
    seed.containerId = QString("1234567890abcdef1234");
    seed.name = QString("web");
    seed.image = QString("nginx:latest");
    seed.state = QString("running");
    seed.memUsage = QString("old");

    const QByteArray payload = R"JSON(
[
  {
    "Id":"1234567890abcdef1234",
    "Name":"/web",
    "Created":"2026-01-01T00:00:00Z",
    "Image":"sha256:img",
    "Config":{
      "Image":"nginx:latest",
      "Hostname":"box",
      "User":"root",
      "WorkingDir":"/work",
      "Entrypoint":["/docker-entrypoint.sh"],
      "Cmd":["nginx","-g","daemon off;"],
      "Env":["A=1","B=2"],
      "Labels":{"app":"easy-ssh"}
    },
    "Driver":"overlay2",
    "HostConfig":{"Runtime":"runc"},
    "RestartCount":2,
    "State":{
      "Status":"running",
      "Pid":456,
      "ExitCode":0,
      "OOMKilled":false,
      "Error":"",
      "StartedAt":"2026-01-01T00:00:05Z",
      "FinishedAt":"0001-01-01T00:00:00Z"
    },
    "Mounts":[{"Source":"/host","Destination":"/data","RW":true}],
    "NetworkSettings":{
      "IPAddress":"172.17.0.2",
      "Gateway":"172.17.0.1",
      "MacAddress":"00:11:22:33:44:55",
      "Ports":{"80/tcp":[{"HostIp":"0.0.0.0","HostPort":"8080"}]}
    }
  }
]
__EASY_SSH_STATS__
{"cpu":"5.5%","mem_percent":"12.25%","mem_usage":"24MiB / 128MiB"}
)JSON";

    ContainerInspectInfo inspect;
    QString error;
    QVERIFY(ContainerParser::parseInspect(payload, seed, &inspect, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(inspect.base.containerId, QString("1234567890abcdef1234"));
    QCOMPARE(inspect.base.name, QString("web"));
    QCOMPARE(inspect.base.pid, 456LL);
    QVERIFY(qAbs(inspect.base.cpuPercent - 5.5) < 0.0001);
    QVERIFY(qAbs(inspect.base.memPercent - 12.25) < 0.0001);
    QCOMPARE(inspect.base.memUsage, QString("24MiB / 128MiB"));
    QCOMPARE(inspect.imageName, QString("nginx:latest"));
    QCOMPARE(inspect.driver, QString("overlay2"));
    QCOMPARE(inspect.ociRuntime, QString("runc"));
    QCOMPARE(inspect.workingDir, QString("/work"));
    QCOMPARE(inspect.entrypoint, QString("/docker-entrypoint.sh"));
    QCOMPARE(inspect.command, QString("nginx -g daemon off;"));
    QCOMPARE(inspect.env.size(), 2);
    QCOMPARE(inspect.mounts.size(), 1);
    QVERIFY(inspect.ports.contains("8080"));
}

QTEST_GUILESS_MAIN(ContainerParserTest)

#include "tst_ContainerParser.moc"
