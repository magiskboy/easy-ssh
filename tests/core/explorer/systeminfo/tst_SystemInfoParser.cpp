// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/explorer/systeminfo/SystemInfoParser.h"

#include <QtTest/QtTest>

class SystemInfoParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void formatHelpers();
    void classifyFailure();
    void fetchCommand();
    void formatSnapshotTextAndJson();
    void parseSnapshotRejectsInvalidInput();
    void parseSnapshotParsesMinimalPayload();
};

void SystemInfoParserTest::formatHelpers()
{
    QCOMPARE(SystemInfoParser::formatBytes(512), QString("512 B"));
    QCOMPARE(SystemInfoParser::formatBytesFromKiB(1536), QString("1.5 MiB"));
    QCOMPARE(SystemInfoParser::formatUptime(3661), QString("1h 1m"));
    QCOMPARE(SystemInfoParser::formatPercent(12.34), QString("12.3%"));
    QCOMPARE(SystemInfoParser::formatRateBps(2048), QString("2.0 KiB/s"));
    QCOMPARE(SystemInfoParser::formatFreqKHz(2400000), QString("2.40 GHz"));
    QCOMPARE(SystemInfoParser::formatIops(12.34), QString("12.3"));
    QCOMPARE(SystemInfoParser::formatCelsius(41.25), QString("41.3 \u00b0C"));
    QCOMPARE(SystemInfoParser::formatLinkSpeed(2500), QString("2.5 Gbps"));
    QCOMPARE(SystemInfoParser::formatLinkSpeed(-1), QString("\u2014"));
}

void SystemInfoParserTest::classifyFailure()
{
    QString message;

    QCOMPARE(SystemInfoParser::classifyFailure(127, QByteArray(""), QString(), &message),
             ExplorerCapability::Unavailable);
    QVERIFY(message.contains("not available"));

    QCOMPARE(SystemInfoParser::classifyFailure(
                 1, QByteArray("permission denied reading /proc"), QString(), &message),
             ExplorerCapability::PermissionDenied);
    QVERIFY(message.contains("Permission denied"));

    QCOMPARE(SystemInfoParser::classifyFailure(2, QByteArray("bad json"), QString(), &message),
             ExplorerCapability::Error);
    QCOMPARE(message, QString("bad json"));
}

void SystemInfoParserTest::fetchCommand()
{
    const QString command = SystemInfoParser::fetchCommand();
    QVERIFY(command.contains(QStringLiteral("/proc/stat")));
    QVERIFY(command.contains(QStringLiteral("/proc/meminfo")));
    QVERIFY(command.contains(QStringLiteral("no /proc")));
}

void SystemInfoParserTest::formatSnapshotTextAndJson()
{
    SystemInfo info;
    info.os.prettyName = QStringLiteral("Fedora");
    info.os.kernel = QStringLiteral("Linux 6.0");
    info.os.arch = QStringLiteral("x86_64");
    info.os.hostname = QStringLiteral("box");
    info.os.uptimeSec = 3661;
    info.load.load1 = 0.1;
    info.load.load5 = 0.2;
    info.load.load15 = 0.3;
    info.cpu.model = QStringLiteral("Ryzen");
    info.cpu.logicalCpus = 2;
    info.cpu.usagePercent = 12.5;
    info.mem.totalKb = 1024;
    info.mem.availableKb = 512;
    info.mem.freeKb = 256;
    info.virt.detectVirt = QStringLiteral("kvm");
    info.virt.vm = QStringLiteral("kvm");

    const QString text = SystemInfoParser::formatSnapshotText(info);
    QVERIFY(text.contains(QStringLiteral("=== System Info ===")));
    QVERIFY(text.contains(QStringLiteral("Fedora")));
    QVERIFY(text.contains(QStringLiteral("Ryzen")));
    QVERIFY(text.contains(QStringLiteral("kvm")));

    const QString json = SystemInfoParser::formatSnapshotJson(info);
    QVERIFY(json.contains(QStringLiteral("\"pretty\": \"Fedora\"")));
    QVERIFY(json.contains(QStringLiteral("\"usage_percent\": 12.5")));
    QVERIFY(json.contains(QStringLiteral("\"detect\": \"kvm\"")));
}

void SystemInfoParserTest::parseSnapshotRejectsInvalidInput()
{
    SystemInfo info;
    QString error;

    QVERIFY(!SystemInfoParser::parseSnapshot(QByteArray(), &info, &error));
    QVERIFY(error.contains("Empty system info response"));

    QVERIFY(!SystemInfoParser::parseSnapshot(QByteArray("{oops"), &info, &error));
    QVERIFY(error.contains("Invalid system info JSON"));
}

void SystemInfoParserTest::parseSnapshotParsesMinimalPayload()
{
    const QByteArray payload = R"JSON(
{
  "os": {"pretty":"Fedora","kernel":"Linux 6.0","arch":"x86_64","hostname":"box","uptime_sec":3661},
  "load": {"1":0.12,"5":0.34,"15":0.56},
  "cpu": {
    "model":"Ryzen",
    "logical":0,
    "governor":"schedutil",
    "freq_min_khz":800000,
    "freq_max_khz":3200000,
    "freq_khz":[2400000,2300000],
    "stat":{"agg":[1,2,3,4,5,6,7,8],"cores":[[11,12,13,14,15,16,17,18],[21,22,23,24,25,26,27,28]]}
  },
  "mem":{"MemTotal":1024,"MemAvailable":512,"MemFree":256,"Buffers":16,"Cached":32,"Shmem":4,"SReclaimable":8,"SwapTotal":64,"SwapFree":48},
  "disks":[{"fs":"/dev/sda1","mount":"/","size_kb":100,"used_kb":40,"avail_kb":60,"use_pct":40}],
  "disk_io":[{"name":"sda","reads":1,"writes":2,"sectors_read":3,"sectors_written":4,"io_ticks":5}],
  "temps":[{"name":"cpu","celsius":-300.0}],
  "gpus":[{"index":0,"name":"RTX","uuid":"GPU-1","pci":"0000:01:00.0","driver":"550","pstate":"P0","util_gpu":10.5,"util_mem":20.5,"mem_total":8192,"mem_used":1024,"mem_free":7168,"temp":65.5,"power_draw":90.1,"power_limit":120.0,"clock_sm":1500,"clock_mem":9000}],
  "gpu_procs":[{"pid":123,"name":"python","gpu_uuid":"GPU-1","used_mem":512}],
  "nics":[{"name":"eth0","mac":"00:11","state":"up","ipv4":"10.0.0.1","ipv6":"fe80::1","speed_mbps":2500,"mtu":1500,"rx_bytes":100,"tx_bytes":200,"rx_packets":3,"tx_packets":4,"rx_errors":5,"tx_errors":6}],
  "virt":{"detect":"kvm","vm":"kvm","container":"","is_vm":true,"is_container":false,"cpu_hypervisor":true,"cpu_vendor":"AMD","dmi_sys_vendor":"QEMU","dmi_product_name":"Standard PC","docker_env":false,"podman_env":false,"wsl":false,"cgroup_init":"0::/"}
}
)JSON";

    SystemInfo info;
    QString error;
    QVERIFY(SystemInfoParser::parseSnapshot(payload, &info, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QCOMPARE(info.os.prettyName, QString("Fedora"));
    QCOMPARE(info.os.uptimeSec, 3661);
    QCOMPARE(info.cpu.logicalCpus, 2);
    QCOMPARE(info.cpu.aggregate.user, 1ULL);
    QCOMPARE(info.cpu.cores.size(), 2);
    QCOMPARE(info.cpu.coreFreqKHz.value(1), 2300000LL);
    QCOMPARE(info.mem.totalKb, 1024ULL);
    QCOMPARE(info.disks.size(), 1);
    QCOMPARE(info.disks.first().filesystem, QString("/dev/sda1"));
    QCOMPARE(info.diskIo.first().name, QString("sda"));
    QCOMPARE(info.temps.first().celsius, -1.0);
    QCOMPARE(info.gpus.first().name, QString("RTX"));
    QCOMPARE(info.gpuProcesses.first().usedMemoryMiB, 512LL);
    QCOMPARE(info.nics.first().speedMbps, 2500LL);
    QVERIFY(info.virt.isVm);
    QCOMPARE(info.virt.dmiSysVendor, QString("QEMU"));
}

QTEST_GUILESS_MAIN(SystemInfoParserTest)

#include "tst_SystemInfoParser.moc"
