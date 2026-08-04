// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SystemInfoSource.h"

#include "core/explorer/systeminfo/SystemInfoParser.h"
#include "core/session/Session.h"

#include <QHash>
#include <QTimer>
#include <QUuid>

namespace
{
quint64 busyTicks(const CpuCoreTicks &t)
{
    return t.user + t.nice + t.system + t.irq + t.softirq + t.steal;
}

quint64 totalTicks(const CpuCoreTicks &t)
{
    return busyTicks(t) + t.idle + t.iowait;
}

double usageFromDelta(const CpuCoreTicks &prev, const CpuCoreTicks &cur)
{
    if (busyTicks(cur) < busyTicks(prev) || totalTicks(cur) < totalTicks(prev)) {
        return -1.0;
    }
    const quint64 busyDelta = busyTicks(cur) - busyTicks(prev);
    const quint64 totalDelta = totalTicks(cur) - totalTicks(prev);
    if (totalDelta == 0) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(busyDelta) / static_cast<double>(totalDelta);
}
} // namespace

SystemInfoSource::SystemInfoSource(Session *session, QObject *parent)
    : IExplorerSource(parent), m_session(session)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_pollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SystemInfoSource::onPollTick);

    if (m_session) {
        connect(m_session,
                &Session::commandFinished,
                this,
                [this](const QString &requestId,
                       int exitStatus,
                       const QByteArray &standardOut,
                       const QByteArray &standardError,
                       const QString &errorMessage) {
                    handleCommandFinished(requestId,
                                          exitStatus,
                                          CommandStreams{standardOut, standardError},
                                          errorMessage);
                });
    }
}

SystemInfoSource::~SystemInfoSource()
{
    stop();
}

void SystemInfoSource::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qMax(500, ms);
    m_timer->setInterval(m_pollIntervalMs);
}

void SystemInfoSource::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_prev.reset();
    setCapability(ExplorerCapability::Checking, tr("Checking…"));
    requestFetch();
    m_timer->start();
}

void SystemInfoSource::stop()
{
    m_running = false;
    m_timer->stop();
    m_activeRequestId.clear();
    m_requestInFlight = false;
    m_refreshAfterInFlight = false;
    m_prev.reset();
    setBusy(false);
}

void SystemInfoSource::refresh()
{
    if (!m_running) {
        start();
        return;
    }
    if (m_requestInFlight) {
        m_refreshAfterInFlight = true;
        return;
    }
    requestFetch();
}

void SystemInfoSource::onPollTick()
{
    if (!m_running || m_requestInFlight) {
        return;
    }
    requestFetch();
}

void SystemInfoSource::requestFetch()
{
    if (!m_session) {
        setCapability(ExplorerCapability::Error, tr("No session"));
        setBusy(false);
        return;
    }

    m_activeRequestId = makeRequestId();
    m_requestInFlight = true;
    setBusy(true);
    m_session->execCommand(m_activeRequestId, SystemInfoParser::fetchCommand());
}

void SystemInfoSource::handleCommandFinished(const QString &requestId,
                                             int exitStatus,
                                             const CommandStreams &streams,
                                             const QString &errorMessage)
{
    if (requestId != m_activeRequestId) {
        return;
    }

    m_activeRequestId.clear();
    m_requestInFlight = false;
    setBusy(false);

    if (!m_running) {
        return;
    }

    const bool transportFailed = !errorMessage.isEmpty() && exitStatus < 0;
    if (transportFailed || exitStatus != 0) {
        QString message;
        const ExplorerCapability cap = SystemInfoParser::classifyFailure(
            exitStatus, streams.standardError, errorMessage, &message);
        setCapability(cap, message);
        if (cap == ExplorerCapability::Error) {
            emit failed(message);
        }
        if (m_refreshAfterInFlight) {
            m_refreshAfterInFlight = false;
            requestFetch();
        }
        return;
    }

    SystemInfo info;
    QString parseError;
    if (!SystemInfoParser::parseSnapshot(streams.standardOut, &info, &parseError)) {
        setCapability(ExplorerCapability::Error,
                      parseError.isEmpty() ? tr("Failed to parse system info") : parseError);
        emit failed(m_capabilityMessage);
        return;
    }

    applyClientDeltas(&info);
    setCapability(ExplorerCapability::Available);
    emit snapshotReady(info);

    if (m_refreshAfterInFlight) {
        m_refreshAfterInFlight = false;
        requestFetch();
    }
}

void SystemInfoSource::applyClientDeltas(SystemInfo *info)
{
    if (!info) {
        return;
    }

    info->cpu.usagePercent = -1.0;
    info->cpu.coreUsagePercent.fill(-1.0, info->cpu.cores.size());
    for (NicInfo &nic : info->nics) {
        nic.rxBps = -1.0;
        nic.txBps = -1.0;
    }
    for (DiskIoInfo &io : info->diskIo) {
        io.readBps = -1.0;
        io.writeBps = -1.0;
        io.readIops = -1.0;
        io.writeIops = -1.0;
        io.utilPercent = -1.0;
    }

    if (m_prev.has_value() && m_sampleTimer.isValid()) {
        const qint64 elapsedMs = m_sampleTimer.elapsed();
        if (elapsedMs > 0) {
            const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;

            info->cpu.usagePercent = usageFromDelta(m_prev->cpu.aggregate, info->cpu.aggregate);

            const int coreCount = qMin(m_prev->cpu.cores.size(), info->cpu.cores.size());
            info->cpu.coreUsagePercent.resize(info->cpu.cores.size());
            for (int i = 0; i < info->cpu.cores.size(); ++i) {
                if (i < coreCount) {
                    info->cpu.coreUsagePercent[i] =
                        usageFromDelta(m_prev->cpu.cores.at(i), info->cpu.cores.at(i));
                } else {
                    info->cpu.coreUsagePercent[i] = -1.0;
                }
            }

            QHash<QString, int> prevNicIndex;
            for (int i = 0; i < m_prev->nics.size(); ++i) {
                prevNicIndex.insert(m_prev->nics.at(i).name, i);
            }
            for (NicInfo &nic : info->nics) {
                const auto it = prevNicIndex.constFind(nic.name);
                if (it == prevNicIndex.cend()) {
                    continue;
                }
                const NicInfo &prev = m_prev->nics.at(it.value());
                if (nic.rxBytes >= prev.rxBytes) {
                    nic.rxBps = static_cast<double>(nic.rxBytes - prev.rxBytes) / elapsedSec;
                }
                if (nic.txBytes >= prev.txBytes) {
                    nic.txBps = static_cast<double>(nic.txBytes - prev.txBytes) / elapsedSec;
                }
            }

            QHash<QString, int> prevDiskIndex;
            for (int i = 0; i < m_prev->diskIo.size(); ++i) {
                prevDiskIndex.insert(m_prev->diskIo.at(i).name, i);
            }
            constexpr double kSectorBytes = 512.0;
            for (DiskIoInfo &io : info->diskIo) {
                const auto it = prevDiskIndex.constFind(io.name);
                if (it == prevDiskIndex.cend()) {
                    continue;
                }
                const DiskIoInfo &prev = m_prev->diskIo.at(it.value());
                if (io.sectorsRead >= prev.sectorsRead) {
                    io.readBps = static_cast<double>(io.sectorsRead - prev.sectorsRead) *
                                 kSectorBytes / elapsedSec;
                }
                if (io.sectorsWritten >= prev.sectorsWritten) {
                    io.writeBps = static_cast<double>(io.sectorsWritten - prev.sectorsWritten) *
                                  kSectorBytes / elapsedSec;
                }
                if (io.readsCompleted >= prev.readsCompleted) {
                    io.readIops =
                        static_cast<double>(io.readsCompleted - prev.readsCompleted) / elapsedSec;
                }
                if (io.writesCompleted >= prev.writesCompleted) {
                    io.writeIops =
                        static_cast<double>(io.writesCompleted - prev.writesCompleted) / elapsedSec;
                }
                if (io.ioTicksMs >= prev.ioTicksMs) {
                    // io_ticks is milliseconds of I/O time; util ≈ Δticks / Δwall.
                    io.utilPercent = 100.0 * static_cast<double>(io.ioTicksMs - prev.ioTicksMs) /
                                     static_cast<double>(elapsedMs);
                    if (io.utilPercent > 100.0) {
                        io.utilPercent = 100.0;
                    }
                }
            }
        }
    }

    m_prev = *info;
    m_sampleTimer.restart();
}

void SystemInfoSource::setCapability(ExplorerCapability capability, const QString &message)
{
    const bool changed = m_capability != capability || m_capabilityMessage != message;
    m_capability = capability;
    m_capabilityMessage = message;
    if (changed) {
        emit capabilityChanged(capability);
    }
}

void SystemInfoSource::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(busy);
}

QString SystemInfoSource::makeRequestId()
{
    return QStringLiteral("systeminfo-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}
