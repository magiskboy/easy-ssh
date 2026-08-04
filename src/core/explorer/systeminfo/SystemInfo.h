/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

struct OsInfo
{
    QString prettyName;
    QString kernel;
    QString arch;
    QString hostname;
    qint64 uptimeSec = 0;
};

struct LoadInfo
{
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;
};

struct CpuCoreTicks
{
    int index = -1; ///< -1 for aggregate "cpu " line
    quint64 user = 0;
    quint64 nice = 0;
    quint64 system = 0;
    quint64 idle = 0;
    quint64 iowait = 0;
    quint64 irq = 0;
    quint64 softirq = 0;
    quint64 steal = 0;
};

struct CpuInfo
{
    QString model;
    int logicalCpus = 0;
    CpuCoreTicks aggregate;
    QVector<CpuCoreTicks> cores;
    double usagePercent = -1.0;       ///< Client-side delta; -1 until second sample
    QVector<double> coreUsagePercent; ///< Per-core; -1 until second sample
    QString governor;
    qint64 freqMinKHz = 0;
    qint64 freqMaxKHz = 0;
    QVector<qint64> coreFreqKHz; ///< Current scaling frequency per core (0 if unknown)
};

struct MemInfo
{
    quint64 totalKb = 0;
    quint64 availableKb = 0;
    quint64 freeKb = 0;
    quint64 buffersKb = 0;
    quint64 cachedKb = 0;
    quint64 shmemKb = 0;
    quint64 sReclaimableKb = 0;
    quint64 swapTotalKb = 0;
    quint64 swapFreeKb = 0;
};

struct DiskInfo
{
    QString filesystem;
    QString mountpoint;
    quint64 sizeKb = 0;
    quint64 usedKb = 0;
    quint64 availKb = 0;
    int usePercent = 0;
};

struct DiskIoInfo
{
    QString name;
    quint64 readsCompleted = 0;
    quint64 writesCompleted = 0;
    quint64 sectorsRead = 0;
    quint64 sectorsWritten = 0;
    quint64 ioTicksMs = 0; ///< Time spent doing I/O (ms) since boot
    double readBps = -1.0;
    double writeBps = -1.0;
    double readIops = -1.0;
    double writeIops = -1.0;
    double utilPercent = -1.0;
};

struct NicInfo
{
    QString name;
    QString mac;
    QString operState;
    QString ipv4;          ///< Comma-separated addresses (may be empty)
    QString ipv6;          ///< Comma-separated global addresses (may be empty)
    qint64 speedMbps = -1; ///< Link speed from sysfs; -1 if unknown
    int mtu = 0;
    quint64 rxBytes = 0;
    quint64 txBytes = 0;
    quint64 rxPackets = 0;
    quint64 txPackets = 0;
    quint64 rxErrors = 0;
    quint64 txErrors = 0;
    double rxBps = -1.0;
    double txBps = -1.0;
};

struct TempSensorInfo
{
    QString name;
    double celsius = -1.0; ///< -1 if unread / invalid
};

struct GpuInfo
{
    int index = -1;
    QString name;
    QString uuid;
    QString pciBusId;
    QString driverVersion;
    QString pstate;
    double utilGpuPercent = -1.0; ///< % time ≥1 kernel resident (NVML semantics)
    double utilMemPercent = -1.0;
    qint64 memTotalMiB = -1;
    qint64 memUsedMiB = -1;
    qint64 memFreeMiB = -1;
    double tempCelsius = -1.0;
    double powerDrawW = -1.0;
    double powerLimitW = -1.0;
    qint64 clockSmMHz = -1;
    qint64 clockMemMHz = -1;
};

struct GpuProcessInfo
{
    qint64 pid = -1;
    QString name;
    QString gpuUuid;
    qint64 usedMemoryMiB = -1;
};

struct VirtInfo
{
    QString detectVirt; ///< Combined systemd-detect-virt (or "none")
    QString vm;         ///< systemd-detect-virt -v
    QString container;  ///< systemd-detect-virt -c
    bool isVm = false;
    bool isContainer = false;
    bool cpuHypervisorFlag = false;
    QString cpuVendor;
    QString dmiSysVendor;
    QString dmiProductName;
    QString dmiProductVersion;
    QString dmiBoardVendor;
    QString dmiBoardName;
    QString dmiChassisVendor;
    QString dmiChassisType;
    QString dmiBiosVendor;
    QString dmiBiosVersion;
    QString dmiBiosDate;
    bool dockerEnv = false;
    bool podmanEnv = false;
    bool wsl = false;
    QString cgroupInit; ///< Summary from /proc/1/cgroup
};

struct SystemInfo
{
    OsInfo os;
    LoadInfo load;
    CpuInfo cpu;
    MemInfo mem;
    QVector<DiskInfo> disks;
    QVector<DiskIoInfo> diskIo;
    QVector<NicInfo> nics;
    QVector<TempSensorInfo> temps;
    QVector<GpuInfo> gpus;
    QVector<GpuProcessInfo> gpuProcesses;
    VirtInfo virt;
};
