/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#import "ESSSessionController.h"
#import "ESSConnectionMapping.h"
#import "EasySshRuntime.h"

#include "ESSRemoteExecHost.h"

#include "core/connection/Connection.h"
#include "core/explorer/ExplorerTypes.h"
#include "core/explorer/IExplorerSource.h"
#include "core/explorer/IRemoteExec.h"
#include "core/explorer/container/ContainerInfo.h"
#include "core/explorer/container/ContainerInspectInfo.h"
#include "core/explorer/container/ContainerParser.h"
#include "core/explorer/container/ContainerSource.h"
#include "core/explorer/process/ProcessInfo.h"
#include "core/explorer/process/ProcessSource.h"
#include "core/explorer/service/ServiceInfo.h"
#include "core/explorer/service/ServiceInspectInfo.h"
#include "core/explorer/service/ServiceParser.h"
#include "core/explorer/service/ServiceSource.h"
#include "core/explorer/systeminfo/SystemInfo.h"
#include "core/explorer/systeminfo/SystemInfoParser.h"
#include "core/explorer/systeminfo/SystemInfoSource.h"
#include "core/fs/TransferTypes.h"
#include "core/ssh/SshWorker.h"
#include "core/tunnel/Tunnel.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QUuid>
#include <optional>

#include <utility>

namespace {

NSString *qToNS(const QString &s)
{
    return [[NSString alloc] initWithUTF8String:s.toUtf8().constData()];
}

QString nsToQ(NSString *s)
{
    if (s == nil) {
        return {};
    }
    return QString::fromUtf8([s UTF8String]);
}

NSUUID *uuidToNS(const QUuid &id)
{
    if (id.isNull()) {
        return nil;
    }
    return [[NSUUID alloc] initWithUUIDString:qToNS(id.toString(QUuid::WithoutBraces))];
}

QUuid nsToUuid(NSUUID *id)
{
    if (id == nil) {
        return {};
    }
    return QUuid(nsToQ(id.UUIDString));
}

NSDictionary *transferJobToDict(const TransferJob &job)
{
    return @{
        @"direction" : @(static_cast<int>(job.direction)),
        @"localPath" : qToNS(job.localPath),
        @"remoteFinalPath" : qToNS(job.remoteFinalPath),
        @"filepartPath" : qToNS(job.filepartPath),
        @"bytesDone" : @(job.bytesDone),
        @"bytesTotal" : @(job.bytesTotal),
        @"backend" : @(static_cast<int>(job.backend)),
        @"lastMessage" : qToNS(job.lastMessage),
    };
}

NSArray<NSString *> *nsStringArray(NSArray<NSString *> *arr)
{
    return arr ?: @[];
}

QStringList toQStringList(NSArray<NSString *> *arr)
{
    QStringList out;
    for (NSString *s in nsStringArray(arr)) {
        out.append(nsToQ(s));
    }
    return out;
}

NSString *capabilityToNS(ExplorerCapability cap)
{
    switch (cap) {
    case ExplorerCapability::Checking:
        return @"checking";
    case ExplorerCapability::Available:
        return @"available";
    case ExplorerCapability::Unavailable:
        return @"unavailable";
    case ExplorerCapability::PermissionDenied:
        return @"permissionDenied";
    case ExplorerCapability::Error:
        return @"error";
    }
    return @"error";
}

NSDictionary *processToDict(const ProcessInfo &p)
{
    return @{
        @"pid" : @(p.pid),
        @"ppid" : @(p.ppid),
        @"uid" : @(p.uid),
        @"user" : qToNS(p.user),
        @"cpuPercent" : @(p.cpuPercent),
        @"memPercent" : @(p.memPercent),
        @"stateCode" : qToNS(p.stateCode),
        @"nice" : @(p.nice),
        @"priority" : @(p.priority),
        @"elapsedSeconds" : @(p.elapsedSeconds),
        @"cpuTime" : qToNS(p.cpuTime),
        @"rssKiB" : @(p.rssKiB),
        @"vszKiB" : @(p.vszKiB),
        @"comm" : qToNS(p.comm),
        @"command" : qToNS(p.command),
    };
}

NSDictionary *containerToDict(const ContainerInfo &c)
{
    return @{
        @"runtime" : qToNS(c.runtime),
        @"containerId" : qToNS(c.containerId),
        @"name" : qToNS(c.name),
        @"image" : qToNS(c.image),
        @"state" : qToNS(c.state),
        @"pid" : @(c.pid),
        @"runtimeNamespace" : qToNS(c.runtimeNamespace),
        @"cpuPercent" : @(c.cpuPercent),
        @"memPercent" : @(c.memPercent),
        @"memUsage" : qToNS(c.memUsage),
    };
}

NSDictionary *serviceToDict(const ServiceInfo &s)
{
    return @{
        @"manager" : qToNS(s.manager),
        @"unit" : qToNS(s.unit),
        @"description" : qToNS(s.description),
        @"loadState" : qToNS(s.loadState),
        @"activeState" : qToNS(s.activeState),
        @"subState" : qToNS(s.subState),
        @"unitFileState" : qToNS(s.unitFileState),
        @"mainPid" : @(s.mainPid),
    };
}

NSArray *stringListToNS(const QStringList &list)
{
    NSMutableArray *out = [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(list.size())];
    for (const QString &s : list) {
        [out addObject:qToNS(s)];
    }
    return out;
}

NSDictionary *containerInspectToDict(const ContainerInspectInfo &info)
{
    NSMutableDictionary *mounts = [NSMutableDictionary dictionary];
    for (const auto &pair : info.mounts) {
        mounts[qToNS(pair.first)] = qToNS(pair.second);
    }
    NSMutableDictionary *dict = [containerToDict(info.base) mutableCopy];
    [dict addEntriesFromDictionary:@{
        @"createdAt" : qToNS(info.createdAt),
        @"startedAt" : qToNS(info.startedAt),
        @"finishedAt" : qToNS(info.finishedAt),
        @"imageId" : qToNS(info.imageId),
        @"imageName" : qToNS(info.imageName),
        @"driver" : qToNS(info.driver),
        @"ociRuntime" : qToNS(info.ociRuntime),
        @"hostname" : qToNS(info.hostname),
        @"user" : qToNS(info.user),
        @"workingDir" : qToNS(info.workingDir),
        @"entrypoint" : qToNS(info.entrypoint),
        @"command" : qToNS(info.command),
        @"env" : stringListToNS(info.env),
        @"mounts" : mounts,
        @"ipAddress" : qToNS(info.ipAddress),
        @"gateway" : qToNS(info.gateway),
        @"macAddress" : qToNS(info.macAddress),
        @"ports" : qToNS(info.ports),
        @"exitCode" : @(info.exitCode),
        @"restartCount" : @(info.restartCount),
        @"oomKilled" : @(info.oomKilled),
        @"stateError" : qToNS(info.stateError),
        @"labels" : qToNS(info.labels),
    }];
    return dict;
}

NSDictionary *serviceInspectToDict(const ServiceInspectInfo &info)
{
    NSMutableDictionary *dict = [serviceToDict(info.base) mutableCopy];
    [dict addEntriesFromDictionary:@{
        @"fragmentPath" : qToNS(info.fragmentPath),
        @"activeEnterTimestamp" : qToNS(info.activeEnterTimestamp),
        @"execMainStartTimestamp" : qToNS(info.execMainStartTimestamp),
        @"type" : qToNS(info.type),
        @"restart" : qToNS(info.restart),
        @"remainAfterExit" : @(info.remainAfterExit),
    }];
    return dict;
}

NSDictionary *cpuTicksToDict(const CpuCoreTicks &t)
{
    return @{
        @"index" : @(t.index),
        @"user" : @(t.user),
        @"nice" : @(t.nice),
        @"system" : @(t.system),
        @"idle" : @(t.idle),
        @"iowait" : @(t.iowait),
        @"irq" : @(t.irq),
        @"softirq" : @(t.softirq),
        @"steal" : @(t.steal),
    };
}

NSDictionary *systemInfoToDict(const SystemInfo &info)
{
    NSMutableArray *cores = [NSMutableArray array];
    for (const CpuCoreTicks &t : info.cpu.cores) {
        [cores addObject:cpuTicksToDict(t)];
    }
    NSMutableArray *coreUsage = [NSMutableArray array];
    for (double v : info.cpu.coreUsagePercent) {
        [coreUsage addObject:@(v)];
    }
    NSMutableArray *coreFreq = [NSMutableArray array];
    for (qint64 v : info.cpu.coreFreqKHz) {
        [coreFreq addObject:@(v)];
    }
    NSMutableArray *disks = [NSMutableArray array];
    for (const DiskInfo &d : info.disks) {
        [disks addObject:@{
            @"filesystem" : qToNS(d.filesystem),
            @"mountpoint" : qToNS(d.mountpoint),
            @"sizeKb" : @(d.sizeKb),
            @"usedKb" : @(d.usedKb),
            @"availKb" : @(d.availKb),
            @"usePercent" : @(d.usePercent),
        }];
    }
    NSMutableArray *diskIo = [NSMutableArray array];
    for (const DiskIoInfo &d : info.diskIo) {
        [diskIo addObject:@{
            @"name" : qToNS(d.name),
            @"readsCompleted" : @(d.readsCompleted),
            @"writesCompleted" : @(d.writesCompleted),
            @"sectorsRead" : @(d.sectorsRead),
            @"sectorsWritten" : @(d.sectorsWritten),
            @"ioTicksMs" : @(d.ioTicksMs),
            @"readBps" : @(d.readBps),
            @"writeBps" : @(d.writeBps),
            @"readIops" : @(d.readIops),
            @"writeIops" : @(d.writeIops),
            @"utilPercent" : @(d.utilPercent),
        }];
    }
    NSMutableArray *nics = [NSMutableArray array];
    for (const NicInfo &n : info.nics) {
        [nics addObject:@{
            @"name" : qToNS(n.name),
            @"mac" : qToNS(n.mac),
            @"operState" : qToNS(n.operState),
            @"ipv4" : qToNS(n.ipv4),
            @"ipv6" : qToNS(n.ipv6),
            @"speedMbps" : @(n.speedMbps),
            @"mtu" : @(n.mtu),
            @"rxBytes" : @(n.rxBytes),
            @"txBytes" : @(n.txBytes),
            @"rxPackets" : @(n.rxPackets),
            @"txPackets" : @(n.txPackets),
            @"rxErrors" : @(n.rxErrors),
            @"txErrors" : @(n.txErrors),
            @"rxBps" : @(n.rxBps),
            @"txBps" : @(n.txBps),
        }];
    }
    NSMutableArray *temps = [NSMutableArray array];
    for (const TempSensorInfo &t : info.temps) {
        [temps addObject:@{@"name" : qToNS(t.name), @"celsius" : @(t.celsius)}];
    }
    NSMutableArray *gpus = [NSMutableArray array];
    for (const GpuInfo &g : info.gpus) {
        [gpus addObject:@{
            @"index" : @(g.index),
            @"name" : qToNS(g.name),
            @"uuid" : qToNS(g.uuid),
            @"pciBusId" : qToNS(g.pciBusId),
            @"driverVersion" : qToNS(g.driverVersion),
            @"pstate" : qToNS(g.pstate),
            @"utilGpuPercent" : @(g.utilGpuPercent),
            @"utilMemPercent" : @(g.utilMemPercent),
            @"memTotalMiB" : @(g.memTotalMiB),
            @"memUsedMiB" : @(g.memUsedMiB),
            @"memFreeMiB" : @(g.memFreeMiB),
            @"tempCelsius" : @(g.tempCelsius),
            @"powerDrawW" : @(g.powerDrawW),
            @"powerLimitW" : @(g.powerLimitW),
            @"clockSmMHz" : @(g.clockSmMHz),
            @"clockMemMHz" : @(g.clockMemMHz),
        }];
    }
    NSMutableArray *gpuProcesses = [NSMutableArray array];
    for (const GpuProcessInfo &p : info.gpuProcesses) {
        [gpuProcesses addObject:@{
            @"pid" : @(p.pid),
            @"name" : qToNS(p.name),
            @"gpuUuid" : qToNS(p.gpuUuid),
            @"usedMemoryMiB" : @(p.usedMemoryMiB),
        }];
    }

    return @{
        @"os" : @{
            @"prettyName" : qToNS(info.os.prettyName),
            @"kernel" : qToNS(info.os.kernel),
            @"arch" : qToNS(info.os.arch),
            @"hostname" : qToNS(info.os.hostname),
            @"uptimeSec" : @(info.os.uptimeSec),
        },
        @"load" : @{
            @"load1" : @(info.load.load1),
            @"load5" : @(info.load.load5),
            @"load15" : @(info.load.load15),
        },
        @"cpu" : @{
            @"model" : qToNS(info.cpu.model),
            @"logicalCpus" : @(info.cpu.logicalCpus),
            @"aggregate" : cpuTicksToDict(info.cpu.aggregate),
            @"cores" : cores,
            @"usagePercent" : @(info.cpu.usagePercent),
            @"coreUsagePercent" : coreUsage,
            @"governor" : qToNS(info.cpu.governor),
            @"freqMinKHz" : @(info.cpu.freqMinKHz),
            @"freqMaxKHz" : @(info.cpu.freqMaxKHz),
            @"coreFreqKHz" : coreFreq,
        },
        @"mem" : @{
            @"totalKb" : @(info.mem.totalKb),
            @"availableKb" : @(info.mem.availableKb),
            @"freeKb" : @(info.mem.freeKb),
            @"buffersKb" : @(info.mem.buffersKb),
            @"cachedKb" : @(info.mem.cachedKb),
            @"shmemKb" : @(info.mem.shmemKb),
            @"sReclaimableKb" : @(info.mem.sReclaimableKb),
            @"swapTotalKb" : @(info.mem.swapTotalKb),
            @"swapFreeKb" : @(info.mem.swapFreeKb),
        },
        @"disks" : disks,
        @"diskIo" : diskIo,
        @"nics" : nics,
        @"temps" : temps,
        @"gpus" : gpus,
        @"gpuProcesses" : gpuProcesses,
        @"virt" : @{
            @"detectVirt" : qToNS(info.virt.detectVirt),
            @"vm" : qToNS(info.virt.vm),
            @"container" : qToNS(info.virt.container),
            @"isVm" : @(info.virt.isVm),
            @"isContainer" : @(info.virt.isContainer),
            @"cpuHypervisorFlag" : @(info.virt.cpuHypervisorFlag),
            @"cpuVendor" : qToNS(info.virt.cpuVendor),
            @"dmiSysVendor" : qToNS(info.virt.dmiSysVendor),
            @"dmiProductName" : qToNS(info.virt.dmiProductName),
            @"dmiProductVersion" : qToNS(info.virt.dmiProductVersion),
            @"dmiBoardVendor" : qToNS(info.virt.dmiBoardVendor),
            @"dmiBoardName" : qToNS(info.virt.dmiBoardName),
            @"dmiChassisVendor" : qToNS(info.virt.dmiChassisVendor),
            @"dmiChassisType" : qToNS(info.virt.dmiChassisType),
            @"dmiBiosVendor" : qToNS(info.virt.dmiBiosVendor),
            @"dmiBiosVersion" : qToNS(info.virt.dmiBiosVersion),
            @"dmiBiosDate" : qToNS(info.virt.dmiBiosDate),
            @"dockerEnv" : @(info.virt.dockerEnv),
            @"podmanEnv" : @(info.virt.podmanEnv),
            @"wsl" : @(info.virt.wsl),
            @"cgroupInit" : qToNS(info.virt.cgroupInit),
        },
    };
}

ContainerInfo containerFromSeed(NSDictionary *seed)
{
    ContainerInfo info;
    info.runtime = nsToQ(seed[@"runtime"] ?: @"");
    info.containerId = nsToQ(seed[@"containerId"] ?: @"");
    info.name = nsToQ(seed[@"name"] ?: @"");
    info.image = nsToQ(seed[@"image"] ?: @"");
    info.state = nsToQ(seed[@"state"] ?: @"");
    info.pid = [seed[@"pid"] longLongValue];
    info.runtimeNamespace = nsToQ(seed[@"runtimeNamespace"] ?: @"");
    info.cpuPercent = [seed[@"cpuPercent"] doubleValue];
    info.memPercent = [seed[@"memPercent"] doubleValue];
    info.memUsage = nsToQ(seed[@"memUsage"] ?: @"");
    return info;
}

ServiceInfo serviceFromSeed(NSDictionary *seed)
{
    ServiceInfo info;
    info.manager = nsToQ(seed[@"manager"] ?: @"");
    info.unit = nsToQ(seed[@"unit"] ?: @"");
    info.description = nsToQ(seed[@"description"] ?: @"");
    info.loadState = nsToQ(seed[@"loadState"] ?: @"");
    info.activeState = nsToQ(seed[@"activeState"] ?: @"");
    info.subState = nsToQ(seed[@"subState"] ?: @"");
    info.unitFileState = nsToQ(seed[@"unitFileState"] ?: @"");
    info.mainPid = [seed[@"mainPid"] longLongValue];
    return info;
}

void dispatchMain(dispatch_block_t block)
{
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_async(dispatch_get_main_queue(), block);
    }
}

} // namespace

@interface ESSSessionController ()
@property (nonatomic, strong, nullable) NSUUID *primaryShellId;
@property (nonatomic, assign) BOOL connected;
- (void)handleInspectFinished:(const QString &)requestId
                   exitStatus:(int)exitStatus
                       stdout:(const QByteArray &)stdoutBytes
                       stderr:(const QByteArray &)stderrBytes
                        error:(const QString &)errorMessage;
- (void)emitCapability:(NSString *)kind source:(IExplorerSource *)source;
- (void)wireSourceLifecycle:(IExplorerSource *)source kind:(NSString *)kind;
@end

@implementation ESSSessionController {
    QThread *m_thread;
    SshWorker *m_worker;
    Connection m_connection;
    SessionCredentials m_credentials;
    bool m_shuttingDown;
    ESSRemoteExecHost *m_remoteExec;
    ProcessSource *m_processSource;
    ContainerSource *m_containerSource;
    ServiceSource *m_serviceSource;
    SystemInfoSource *m_systemInfoSource;
    std::optional<SystemInfo> m_lastSystemInfo;
    QString m_pendingContainerInspectId;
    ContainerInfo m_pendingContainerSeed;
    QString m_pendingServiceInspectId;
    ServiceInfo m_pendingServiceSeed;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [EasySshRuntime start];
        m_thread = nullptr;
        m_worker = nullptr;
        m_shuttingDown = false;
        m_remoteExec = new ESSRemoteExecHost();
        m_processSource = nullptr;
        m_containerSource = nullptr;
        m_serviceSource = nullptr;
        m_systemInfoSource = nullptr;
        QObject::connect(m_remoteExec,
                         &IRemoteExec::commandFinished,
                         m_remoteExec,
                         [self](const QString &requestId,
                                int exitStatus,
                                const QByteArray &stdoutBytes,
                                const QByteArray &stderrBytes,
                                const QString &errorMessage) {
                             [self handleInspectFinished:requestId
                                              exitStatus:exitStatus
                                                   stdout:stdoutBytes
                                                   stderr:stderrBytes
                                                    error:errorMessage];
                         });
        _connected = NO;
    }
    return self;
}

- (void)dealloc
{
    [self stopAllExplorers];
    [self shutdownWorker];
    delete m_remoteExec;
    m_remoteExec = nullptr;
}

- (void)shutdownWorker
{
    m_shuttingDown = true;
    [self stopAllExplorers];
    if (m_remoteExec != nullptr) {
        m_remoteExec->clearWorker();
    }
    if (m_worker != nullptr) {
        QObject::disconnect(m_worker, nullptr, nullptr, nullptr);
        m_worker->requestCancel();
        if (m_thread != nullptr && m_thread->isRunning()) {
            QMetaObject::invokeMethod(
                m_worker,
                [worker = m_worker]() { worker->disconnectSession(); },
                Qt::BlockingQueuedConnection);
        } else {
            m_worker->disconnectSession();
        }
    }
    if (m_thread != nullptr) {
        m_thread->quit();
        if (!m_thread->wait(2000)) {
            m_thread->terminate();
            m_thread->wait(1000);
        }
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_worker;
    m_worker = nullptr;
    self.connected = NO;
    self.primaryShellId = nil;
}

- (void)wireWorkerSignals
{
    QObject::connect(m_worker, &SshWorker::connected, m_worker, [self](const QUuid &shellId) {
        NSUUID *nsId = uuidToNS(shellId);
        if (m_remoteExec != nullptr) {
            m_remoteExec->setConnected(true);
        }
        dispatchMain(^{
            self.primaryShellId = nsId;
            self.connected = YES;
            if (self.onConnected) {
                self.onConnected(nsId);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::dataReceived, m_worker,
                     [self](const QUuid &shellId, const QByteArray &data) {
                         if (data.isEmpty()) {
                             return;
                         }
                         NSUUID *nsId = uuidToNS(shellId);
                         NSData *nsData = [NSData dataWithBytes:data.constData() length:static_cast<NSUInteger>(data.size())];
                         dispatchMain(^{
                             if (self.onData) {
                                 self.onData(nsId, nsData);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::shellOpened, m_worker, [self](const QUuid &shellId) {
        NSUUID *nsId = uuidToNS(shellId);
        dispatchMain(^{
            if (self.onShellOpened) {
                self.onShellOpened(nsId);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::shellClosed, m_worker, [self](const QUuid &shellId) {
        NSUUID *nsId = uuidToNS(shellId);
        dispatchMain(^{
            if (self.primaryShellId != nil && [self.primaryShellId isEqual:nsId]) {
                self.primaryShellId = nil;
            }
            if (self.onShellClosed) {
                self.onShellClosed(nsId);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::shellFailed, m_worker,
                     [self](const QUuid &shellId, const QString &message) {
                         NSUUID *nsId = uuidToNS(shellId);
                         NSString *msg = qToNS(message);
                         dispatchMain(^{
                             if (self.onShellFailed) {
                                 self.onShellFailed(nsId, msg);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::hostKeyPrompt, m_worker,
                     [self](SshWorker::HostKeyPrompt reason, const QString &fingerprint,
                            const QString &contextLabel) {
                         ESSHostKeyPromptReason r = ESSHostKeyPromptReasonUnknown;
                         switch (reason) {
                         case SshWorker::HostKeyPrompt::Changed:
                             r = ESSHostKeyPromptReasonChanged;
                             break;
                         case SshWorker::HostKeyPrompt::Other:
                             r = ESSHostKeyPromptReasonOther;
                             break;
                         case SshWorker::HostKeyPrompt::Unknown:
                         default:
                             r = ESSHostKeyPromptReasonUnknown;
                             break;
                         }
                         NSString *fp = qToNS(fingerprint);
                         NSString *ctx = qToNS(contextLabel);
                         dispatchMain(^{
                             if (self.onHostKeyPrompt) {
                                 self.onHostKeyPrompt(r, fp, ctx);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::errorOccurred, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            self.connected = NO;
            if (self.onError) {
                self.onError(msg);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::disconnected, m_worker, [self]() {
        if (m_remoteExec != nullptr) {
            m_remoteExec->setConnected(false);
        }
        dispatchMain(^{
            [self stopAllExplorers];
            self.connected = NO;
            self.primaryShellId = nil;
            if (self.onDisconnected) {
                self.onDisconnected();
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::agentForwardingWarning, m_worker,
                     [self](const QString &message) {
                         NSString *msg = qToNS(message);
                         dispatchMain(^{
                             if (self.onAgentForwardingWarning) {
                                 self.onAgentForwardingWarning(msg);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::directoryListed, m_worker,
                     [self](const QString &path, const QVector<RemoteEntry> &entries) {
                         NSString *nsPath = qToNS(path);
                         NSMutableArray *arr = [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(entries.size())];
                         for (const RemoteEntry &e : entries) {
                             [arr addObject:@{
                                 @"name" : qToNS(e.name),
                                 @"path" : qToNS(e.path),
                                 @"isDir" : @(e.isDir),
                                 @"isSymlink" : @(e.isSymlink),
                                 @"linkIsDir" : @(e.linkIsDir),
                                 @"linkTarget" : qToNS(e.linkTarget),
                                 @"size" : @(e.size),
                                 @"permissions" : qToNS(e.permissions),
                                 @"mtime" : @(e.mtime),
                             }];
                         }
                         dispatchMain(^{
                             if (self.onDirectoryListed) {
                                 self.onDirectoryListed(nsPath, arr);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::entryResolved, m_worker,
                     [self](const QString &path, bool isDir, bool ok, const QString &error) {
                         NSString *nsPath = qToNS(path);
                         NSString *nsErr = qToNS(error);
                         dispatchMain(^{
                             if (self.onEntryResolved) {
                                 self.onEntryResolved(nsPath, isDir, ok, nsErr);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::pathCanonicalized, m_worker,
                     [self](const QString &requested, const QString &canonical) {
                         NSString *a = qToNS(requested);
                         NSString *b = qToNS(canonical);
                         dispatchMain(^{
                             if (self.onPathCanonicalized) {
                                 self.onPathCanonicalized(a, b);
                             }
                         });
                     });

    QObject::connect(m_worker, &SshWorker::sftpFinished, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpFinished) {
                self.onSftpFinished(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpError, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpError) {
                self.onSftpError(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpCanceled, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpCanceled) {
                self.onSftpCanceled(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpInterrupted, m_worker, [self](const TransferJob &job) {
        NSDictionary *dict = transferJobToDict(job);
        dispatchMain(^{
            if (self.onSftpInterrupted) {
                self.onSftpInterrupted(dict);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpUnavailable, m_worker, [self](const QString &message) {
        NSString *msg = qToNS(message);
        dispatchMain(^{
            if (self.onSftpUnavailable) {
                self.onSftpUnavailable(msg);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::sftpProgress, m_worker,
                     [self](qint64 done, qint64 total, const QString &name) {
                         NSString *nsName = qToNS(name);
                         dispatchMain(^{
                             if (self.onSftpProgress) {
                                 self.onSftpProgress(done, total, nsName);
                             }
                         });
                     });
    QObject::connect(m_worker, &SshWorker::transferResumableChanged, m_worker, [self](bool resumable) {
        dispatchMain(^{
            if (self.onTransferResumableChanged) {
                self.onTransferResumableChanged(resumable);
            }
        });
    });
    QObject::connect(m_worker, &SshWorker::remoteFsOpened, m_worker, [self](int backend) {
        dispatchMain(^{
            if (self.onRemoteFsOpened) {
                self.onRemoteFsOpened(backend);
            }
        });
    });

    QObject::connect(m_worker, &SshWorker::tunnelStatusChanged, m_worker,
                     [self](const QUuid &tunnelId, const QString &status, const QString &detail) {
                         NSUUID *nsId = uuidToNS(tunnelId);
                         NSString *st = qToNS(status);
                         NSString *dt = qToNS(detail);
                         dispatchMain(^{
                             if (self.onTunnelStatusChanged) {
                                 self.onTunnelStatusChanged(nsId, st, dt);
                             }
                         });
                     });
    QObject::connect(m_worker, &SshWorker::tunnelError, m_worker,
                     [self](const QUuid &tunnelId, const QString &message) {
                         NSUUID *nsId = uuidToNS(tunnelId);
                         NSString *msg = qToNS(message);
                         dispatchMain(^{
                             if (self.onTunnelError) {
                                 self.onTunnelError(nsId, msg);
                             }
                         });
                     });

    QObject::connect(
        m_worker, &SshWorker::commandFinished, m_worker,
        [self](const QString &requestId, int exitStatus, const QByteArray &stdoutBytes,
               const QByteArray &stderrBytes, const QString &errorMessage) {
            NSString *rid = qToNS(requestId);
            NSData *outData = [NSData dataWithBytes:stdoutBytes.constData()
                                             length:static_cast<NSUInteger>(stdoutBytes.size())];
            NSData *errData = [NSData dataWithBytes:stderrBytes.constData()
                                             length:static_cast<NSUInteger>(stderrBytes.size())];
            NSString *err = qToNS(errorMessage);
            dispatchMain(^{
                if (self.onCommandFinished) {
                    self.onCommandFinished(rid, exitStatus, outData, errData, err);
                }
            });
        });
}

- (void)connectWithConnection:(ESSConnectionInfo *)connection
                  credentials:(ESSSessionCredentials *)credentials
                         cols:(NSInteger)cols
                         rows:(NSInteger)rows
{
    [EasySshRuntime start];
    [self shutdownWorker];
    m_shuttingDown = false;

    m_connection = essConnectionFromInfo(connection);
    if (m_connection.name.isEmpty()) {
        m_connection.name =
            QStringLiteral("%1@%2").arg(m_connection.username, m_connection.host);
    }
    m_credentials = essCredentialsFromInfo(credentials);

    m_thread = new QThread();
    m_worker = new SshWorker();
    m_worker->moveToThread(m_thread);

    [self wireWorkerSignals];
    if (m_remoteExec != nullptr) {
        m_remoteExec->setWorker(m_worker);
        m_remoteExec->setConnected(false);
    }

    m_thread->start();

    const int useCols = cols > 0 ? static_cast<int>(cols) : 80;
    const int useRows = rows > 0 ? static_cast<int>(rows) : 24;
    const QUuid shellId = QUuid::createUuid();
    self.primaryShellId = uuidToNS(shellId);

    const Connection conn = m_connection;
    const SessionCredentials creds = m_credentials;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, conn, creds, shellId, useCols, useRows]() {
            worker->connectToHost(conn, creds, shellId, useCols, useRows);
        },
        Qt::QueuedConnection);
}

- (void)connectWithHost:(NSString *)host
                   port:(NSInteger)port
               username:(NSString *)username
               authType:(ESSAuthType)authType
               password:(NSString *)password
         privateKeyPath:(NSString *)privateKeyPath
                   cols:(NSInteger)cols
                   rows:(NSInteger)rows
{
    ESSConnectionInfo *info = [[ESSConnectionInfo alloc] init];
    info.connectionId = [NSUUID UUID];
    info.host = host ?: @"";
    info.port = port > 0 ? port : 22;
    info.username = username ?: @"";
    info.name = [NSString stringWithFormat:@"%@@%@", info.username, info.host];
    info.authType = authType;
    info.privateKeyPath = privateKeyPath;

    ESSSessionCredentials *creds = [[ESSSessionCredentials alloc] init];
    creds.targetSecret = password;

    [self connectWithConnection:info credentials:creds cols:cols rows:rows];
}

- (void)disconnect
{
    [self shutdownWorker];
    dispatchMain(^{
        if (self.onDisconnected) {
            self.onDisconnected();
        }
    });
}

- (void)reconnectWithCols:(NSInteger)cols rows:(NSInteger)rows
{
    if (m_connection.host.isEmpty()) {
        return;
    }
    ESSConnectionInfo *info = essConnectionToInfo(m_connection);
    ESSSessionCredentials *creds = [[ESSSessionCredentials alloc] init];
    if (!m_credentials.targetSecret.isEmpty()) {
        creds.targetSecret = qToNS(m_credentials.targetSecret);
    }
    if (!m_credentials.gatewaySecret.isEmpty()) {
        creds.gatewaySecret = qToNS(m_credentials.gatewaySecret);
    }
    [self connectWithConnection:info credentials:creds cols:cols rows:rows];
}

- (void)respondHostKeyTrust:(BOOL)accept
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, accept]() { worker->respondHostKeyTrust(accept); },
        Qt::QueuedConnection);
}

- (QUuid)resolvedShellId:(NSUUID *)shellId
{
    if (shellId != nil) {
        return nsToUuid(shellId);
    }
    return nsToUuid(self.primaryShellId);
}

- (void)writeData:(NSData *)data shellId:(NSUUID *)shellId
{
    if (m_worker == nullptr || data == nil || data.length == 0) {
        return;
    }
    const QUuid id = [self resolvedShellId:shellId];
    const QByteArray bytes(static_cast<const char *>(data.bytes), static_cast<int>(data.length));
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id, bytes]() { worker->writeToChannel(id, bytes); },
        Qt::QueuedConnection);
}

- (void)resizeCols:(NSInteger)cols rows:(NSInteger)rows shellId:(NSUUID *)shellId
{
    if (m_worker == nullptr) {
        return;
    }
    const QUuid id = [self resolvedShellId:shellId];
    const int c = static_cast<int>(cols);
    const int r = static_cast<int>(rows);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id, c, r]() { worker->changePtySize(id, c, r); },
        Qt::QueuedConnection);
}

- (void)openShell:(NSUUID *)shellId cols:(NSInteger)cols rows:(NSInteger)rows
{
    if (m_worker == nullptr || shellId == nil) {
        return;
    }
    const QUuid id = nsToUuid(shellId);
    const int c = static_cast<int>(cols);
    const int r = static_cast<int>(rows);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id, c, r]() { worker->openShell(id, c, r); },
        Qt::QueuedConnection);
}

- (void)closeShell:(NSUUID *)shellId
{
    if (m_worker == nullptr || shellId == nil) {
        return;
    }
    const QUuid id = nsToUuid(shellId);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id]() { worker->closeShell(id); }, Qt::QueuedConnection);
}

- (void)listDirectory:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->listDirectory(p); }, Qt::QueuedConnection);
}

- (void)createDirectory:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->createDirectory(p); }, Qt::QueuedConnection);
}

- (void)createSymlink:(NSString *)target linkPath:(NSString *)linkPath
{
    if (m_worker == nullptr) {
        return;
    }
    const QString t = nsToQ(target);
    const QString l = nsToQ(linkPath);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, t, l]() { worker->createSymlink(t, l); }, Qt::QueuedConnection);
}

- (void)resolveEntry:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->resolveEntry(p); }, Qt::QueuedConnection);
}

- (void)renamePath:(NSString *)from to:(NSString *)to
{
    if (m_worker == nullptr) {
        return;
    }
    const QString a = nsToQ(from);
    const QString b = nsToQ(to);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, a, b]() { worker->renamePath(a, b); }, Qt::QueuedConnection);
}

- (void)removePath:(NSString *)path recursive:(BOOL)recursive
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p, recursive]() { worker->removePath(p, recursive); },
        Qt::QueuedConnection);
}

- (void)uploadFiles:(NSArray<NSString *> *)localPaths remoteDir:(NSString *)remoteDir
{
    if (m_worker == nullptr) {
        return;
    }
    const QStringList paths = toQStringList(localPaths);
    const QString dir = nsToQ(remoteDir);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, paths, dir]() { worker->uploadFiles(paths, dir); },
        Qt::QueuedConnection);
}

- (void)uploadFileTo:(NSString *)localPath remotePath:(NSString *)remotePath
{
    if (m_worker == nullptr) {
        return;
    }
    const QString local = nsToQ(localPath);
    const QString remote = nsToQ(remotePath);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, local, remote]() { worker->uploadFileTo(local, remote); },
        Qt::QueuedConnection);
}

- (void)downloadPaths:(NSArray<NSString *> *)remotePaths localDir:(NSString *)localDir
{
    [self downloadPaths:remotePaths localDir:localDir followSymlinks:NO];
}

- (void)downloadPaths:(NSArray<NSString *> *)remotePaths
             localDir:(NSString *)localDir
       followSymlinks:(BOOL)followSymlinks
{
    if (m_worker == nullptr) {
        return;
    }
    const QStringList paths = toQStringList(remotePaths);
    const QString dir = nsToQ(localDir);
    const bool follow = followSymlinks ? true : false;
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, paths, dir, follow]() { worker->downloadPaths(paths, dir, follow); },
        Qt::QueuedConnection);
}

- (void)canonicalizePath:(NSString *)path
{
    if (m_worker == nullptr) {
        return;
    }
    const QString p = nsToQ(path);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, p]() { worker->canonicalizePath(p); }, Qt::QueuedConnection);
}

- (void)cancelTransfer
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->cancelTransfer(); }, Qt::QueuedConnection);
}

- (void)resumeInterruptedTransfer
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->resumeInterruptedTransfer(); },
        Qt::QueuedConnection);
}

- (void)discardInterruptedTransfer
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->discardInterruptedTransfer(); },
        Qt::QueuedConnection);
}

- (void)startLocalTunnelNamed:(NSString *)name
                    localHost:(NSString *)localHost
                    localPort:(NSInteger)localPort
                   remoteHost:(NSString *)remoteHost
                   remotePort:(NSInteger)remotePort
{
    if (m_worker == nullptr) {
        return;
    }
    TunnelDefinition def;
    def.id = QUuid::createUuid();
    def.connectionId = m_connection.id;
    def.name = nsToQ(name);
    def.type = TunnelType::Local;
    def.enabled = true;
    def.localHost = nsToQ(localHost);
    def.localPort = static_cast<quint16>(localPort);
    def.remoteHost = nsToQ(remoteHost);
    def.remotePort = static_cast<quint16>(remotePort);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, def]() { worker->startTunnel(def); }, Qt::QueuedConnection);
}

- (void)stopTunnel:(NSUUID *)tunnelId
{
    if (m_worker == nullptr || tunnelId == nil) {
        return;
    }
    const QUuid id = nsToUuid(tunnelId);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, id]() { worker->stopTunnel(id); }, Qt::QueuedConnection);
}

- (void)stopAllTunnels
{
    if (m_worker == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker]() { worker->stopAllTunnels(); }, Qt::QueuedConnection);
}

- (void)execCommand:(NSString *)command requestId:(NSString *)requestId
{
    if (m_worker == nullptr) {
        return;
    }
    const QString cmd = nsToQ(command);
    const QString rid = nsToQ(requestId);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, rid, cmd]() { worker->execCommand(rid, cmd); },
        Qt::QueuedConnection);
}

- (void)emitCapability:(NSString *)kind source:(IExplorerSource *)source
{
    if (source == nullptr) {
        return;
    }
    NSString *cap = capabilityToNS(source->capability());
    NSString *msg = qToNS(source->capabilityMessage());
    dispatchMain(^{
        if (self.onExplorerCapability) {
            self.onExplorerCapability(kind, cap, msg);
        }
    });
}

- (void)wireSourceLifecycle:(IExplorerSource *)source kind:(NSString *)kind
{
    if (source == nullptr) {
        return;
    }
    QObject::connect(source, &IExplorerSource::capabilityChanged, source, [self, kind, source](ExplorerCapability) {
        [self emitCapability:kind source:source];
    });
    QObject::connect(source, &IExplorerSource::busyChanged, source, [self, kind](bool busy) {
        dispatchMain(^{
            if (self.onExplorerBusy) {
                self.onExplorerBusy(kind, busy ? YES : NO);
            }
        });
    });
    QObject::connect(source, &IExplorerSource::failed, source, [self, kind](const QString &error) {
        NSString *msg = qToNS(error);
        dispatchMain(^{
            if (self.onExplorerFailed) {
                self.onExplorerFailed(kind, msg);
            }
        });
    });
}

- (void)startExplorer:(NSString *)kind
{
    if (m_remoteExec == nullptr || kind.length == 0) {
        return;
    }
    if ([kind isEqualToString:@"process"]) {
        if (m_processSource == nullptr) {
            m_processSource = new ProcessSource(m_remoteExec);
            [self wireSourceLifecycle:m_processSource kind:kind];
            QObject::connect(m_processSource,
                             &ProcessSource::snapshotReady,
                             m_processSource,
                             [self](const QVector<ProcessInfo> &rows) {
                                 NSMutableArray *arr =
                                     [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(rows.size())];
                                 for (const ProcessInfo &p : rows) {
                                     [arr addObject:processToDict(p)];
                                 }
                                 dispatchMain(^{
                                     if (self.onProcessSnapshot) {
                                         self.onProcessSnapshot(arr);
                                     }
                                 });
                             });
        }
        m_processSource->start();
        [self emitCapability:kind source:m_processSource];
        return;
    }
    if ([kind isEqualToString:@"container"]) {
        if (m_containerSource == nullptr) {
            m_containerSource = new ContainerSource(m_remoteExec);
            [self wireSourceLifecycle:m_containerSource kind:kind];
            QObject::connect(m_containerSource,
                             &ContainerSource::snapshotReady,
                             m_containerSource,
                             [self](const QVector<ContainerInfo> &rows) {
                                 NSMutableArray *arr =
                                     [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(rows.size())];
                                 for (const ContainerInfo &c : rows) {
                                     [arr addObject:containerToDict(c)];
                                 }
                                 dispatchMain(^{
                                     if (self.onContainerSnapshot) {
                                         self.onContainerSnapshot(arr);
                                     }
                                 });
                             });
        }
        m_containerSource->start();
        [self emitCapability:kind source:m_containerSource];
        return;
    }
    if ([kind isEqualToString:@"service"]) {
        if (m_serviceSource == nullptr) {
            m_serviceSource = new ServiceSource(m_remoteExec);
            [self wireSourceLifecycle:m_serviceSource kind:kind];
            QObject::connect(m_serviceSource,
                             &ServiceSource::snapshotReady,
                             m_serviceSource,
                             [self](const QVector<ServiceInfo> &rows) {
                                 NSMutableArray *arr =
                                     [NSMutableArray arrayWithCapacity:static_cast<NSUInteger>(rows.size())];
                                 for (const ServiceInfo &s : rows) {
                                     [arr addObject:serviceToDict(s)];
                                 }
                                 dispatchMain(^{
                                     if (self.onServiceSnapshot) {
                                         self.onServiceSnapshot(arr);
                                     }
                                 });
                             });
        }
        m_serviceSource->start();
        [self emitCapability:kind source:m_serviceSource];
        return;
    }
    if ([kind isEqualToString:@"systemInfo"]) {
        if (m_systemInfoSource == nullptr) {
            m_systemInfoSource = new SystemInfoSource(m_remoteExec);
            [self wireSourceLifecycle:m_systemInfoSource kind:kind];
            QObject::connect(m_systemInfoSource,
                             &SystemInfoSource::snapshotReady,
                             m_systemInfoSource,
                             [self](const SystemInfo &info) {
                                 m_lastSystemInfo = info;
                                 NSDictionary *dict = systemInfoToDict(info);
                                 dispatchMain(^{
                                     if (self.onSystemInfoSnapshot) {
                                         self.onSystemInfoSnapshot(dict);
                                     }
                                 });
                             });
        }
        m_systemInfoSource->start();
        [self emitCapability:kind source:m_systemInfoSource];
    }
}

- (void)stopExplorer:(NSString *)kind
{
    if ([kind isEqualToString:@"process"] && m_processSource != nullptr) {
        m_processSource->stop();
        delete m_processSource;
        m_processSource = nullptr;
    } else if ([kind isEqualToString:@"container"] && m_containerSource != nullptr) {
        m_containerSource->stop();
        delete m_containerSource;
        m_containerSource = nullptr;
    } else if ([kind isEqualToString:@"service"] && m_serviceSource != nullptr) {
        m_serviceSource->stop();
        delete m_serviceSource;
        m_serviceSource = nullptr;
    } else if ([kind isEqualToString:@"systemInfo"] && m_systemInfoSource != nullptr) {
        m_systemInfoSource->stop();
        delete m_systemInfoSource;
        m_systemInfoSource = nullptr;
    }
}

- (void)refreshExplorer:(NSString *)kind
{
    if ([kind isEqualToString:@"process"] && m_processSource != nullptr) {
        m_processSource->refresh();
    } else if ([kind isEqualToString:@"container"] && m_containerSource != nullptr) {
        m_containerSource->refresh();
    } else if ([kind isEqualToString:@"service"] && m_serviceSource != nullptr) {
        m_serviceSource->refresh();
    } else if ([kind isEqualToString:@"systemInfo"] && m_systemInfoSource != nullptr) {
        m_systemInfoSource->refresh();
    } else {
        [self startExplorer:kind];
    }
}

- (void)stopAllExplorers
{
    [self stopExplorer:@"process"];
    [self stopExplorer:@"container"];
    [self stopExplorer:@"service"];
    [self stopExplorer:@"systemInfo"];
    m_pendingContainerInspectId.clear();
    m_pendingServiceInspectId.clear();
    m_lastSystemInfo.reset();
}

- (void)inspectContainer:(NSDictionary *)seed
{
    if (m_remoteExec == nullptr || seed == nil) {
        dispatchMain(^{
            if (self.onContainerInspect) {
                self.onContainerInspect(@{}, @"No session");
            }
        });
        return;
    }
    m_pendingContainerSeed = containerFromSeed(seed);
    const QString cmd = ContainerParser::inspectCommand(m_pendingContainerSeed);
    if (cmd.isEmpty()) {
        dispatchMain(^{
            if (self.onContainerInspect) {
                self.onContainerInspect(@{}, @"Inspect not supported for this runtime");
            }
        });
        return;
    }
    m_pendingContainerInspectId =
        QStringLiteral("inspect-container-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_remoteExec->execCommand(m_pendingContainerInspectId, cmd);
}

- (void)inspectService:(NSDictionary *)seed
{
    if (m_remoteExec == nullptr || seed == nil) {
        dispatchMain(^{
            if (self.onServiceInspect) {
                self.onServiceInspect(@{}, @"No session");
            }
        });
        return;
    }
    m_pendingServiceSeed = serviceFromSeed(seed);
    const QString cmd = ServiceParser::inspectCommand(m_pendingServiceSeed);
    if (cmd.isEmpty()) {
        dispatchMain(^{
            if (self.onServiceInspect) {
                self.onServiceInspect(@{}, @"Inspect not supported");
            }
        });
        return;
    }
    m_pendingServiceInspectId =
        QStringLiteral("inspect-service-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_remoteExec->execCommand(m_pendingServiceInspectId, cmd);
}

- (void)handleInspectFinished:(const QString &)requestId
                   exitStatus:(int)exitStatus
                       stdout:(const QByteArray &)stdoutBytes
                       stderr:(const QByteArray &)stderrBytes
                        error:(const QString &)errorMessage
{
    Q_UNUSED(stderrBytes);
    if (!m_pendingContainerInspectId.isEmpty() && requestId == m_pendingContainerInspectId) {
        m_pendingContainerInspectId.clear();
        if (!errorMessage.isEmpty() && exitStatus < 0) {
            NSString *err = qToNS(errorMessage);
            dispatchMain(^{
                if (self.onContainerInspect) {
                    self.onContainerInspect(@{}, err);
                }
            });
            return;
        }
        ContainerInspectInfo info;
        QString parseError;
        if (exitStatus != 0 ||
            !ContainerParser::parseInspect(stdoutBytes, m_pendingContainerSeed, &info, &parseError)) {
            NSString *err = qToNS(parseError.isEmpty() ? QStringLiteral("Inspect failed") : parseError);
            dispatchMain(^{
                if (self.onContainerInspect) {
                    self.onContainerInspect(@{}, err);
                }
            });
            return;
        }
        NSDictionary *dict = containerInspectToDict(info);
        dispatchMain(^{
            if (self.onContainerInspect) {
                self.onContainerInspect(dict, nil);
            }
        });
        return;
    }
    if (!m_pendingServiceInspectId.isEmpty() && requestId == m_pendingServiceInspectId) {
        m_pendingServiceInspectId.clear();
        if (!errorMessage.isEmpty() && exitStatus < 0) {
            NSString *err = qToNS(errorMessage);
            dispatchMain(^{
                if (self.onServiceInspect) {
                    self.onServiceInspect(@{}, err);
                }
            });
            return;
        }
        ServiceInspectInfo info;
        QString parseError;
        if (exitStatus != 0 ||
            !ServiceParser::parseInspect(stdoutBytes, m_pendingServiceSeed, &info, &parseError)) {
            NSString *err = qToNS(parseError.isEmpty() ? QStringLiteral("Inspect failed") : parseError);
            dispatchMain(^{
                if (self.onServiceInspect) {
                    self.onServiceInspect(@{}, err);
                }
            });
            return;
        }
        NSDictionary *dict = serviceInspectToDict(info);
        dispatchMain(^{
            if (self.onServiceInspect) {
                self.onServiceInspect(dict, nil);
            }
        });
    }
}

- (NSString *)systemInfoText
{
    if (!m_lastSystemInfo.has_value()) {
        return @"";
    }
    return qToNS(SystemInfoParser::formatSnapshotText(*m_lastSystemInfo));
}

- (NSString *)systemInfoJson
{
    if (!m_lastSystemInfo.has_value()) {
        return @"";
    }
    return qToNS(SystemInfoParser::formatSnapshotJson(*m_lastSystemInfo));
}

- (NSString *)serviceFollowLogsCommand:(NSDictionary *)seed lines:(NSInteger)lines
{
    if (seed == nil) {
        return @"";
    }
    const int useLines = lines > 0 ? static_cast<int>(lines) : 100;
    return qToNS(ServiceParser::followLogsCommand(serviceFromSeed(seed), useLines));
}

@end
