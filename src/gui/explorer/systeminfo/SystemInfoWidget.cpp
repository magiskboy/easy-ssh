// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SystemInfoWidget.h"

#include "core/explorer/systeminfo/SystemInfoParser.h"
#include "core/explorer/systeminfo/SystemInfoSource.h"
#include "core/session/Session.h"
#include "core/session/SessionRemoteExec.h"
#include "gui/widgets/UiMetrics.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace
{
QFrame *makeGroupFrame(const QString &title, QWidget *parent)
{
    auto *group = new QFrame(parent);
    group->setObjectName(QStringLiteral("systemInfoGroup"));
    group->setFrameShape(QFrame::StyledPanel);
    group->setFrameShadow(QFrame::Plain);
    group->setAutoFillBackground(true);
    {
        QPalette groupPalette = group->palette();
        groupPalette.setColor(QPalette::Window, groupPalette.color(QPalette::AlternateBase));
        group->setPalette(groupPalette);
        group->setBackgroundRole(QPalette::Window);
    }

    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(UiMetrics::tightSpacing);

    auto *titleLabel = new QLabel(title, group);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    return group;
}

QLabel *makeValueLabel(QWidget *parent)
{
    auto *label = new QLabel(QStringLiteral("—"), parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

quint64 memUsedKb(const MemInfo &mem)
{
    if (mem.totalKb == 0) {
        return 0;
    }
    if (mem.availableKb > 0 && mem.availableKb <= mem.totalKb) {
        return mem.totalKb - mem.availableKb;
    }
    const quint64 reclaimable = mem.freeKb + mem.buffersKb + mem.cachedKb;
    if (reclaimable <= mem.totalKb) {
        return mem.totalKb - reclaimable;
    }
    return 0;
}
} // namespace

SystemInfoWidget::SystemInfoWidget(Session *session, QWidget *parent)
    : QWidget(parent), m_session(session)
{
    buildUi();

    if (!m_session || m_session->state() != SessionState::Connected) {
        setStatus(tr("No connected session."), true);
        m_tabs->setEnabled(false);
        updateCopyEnabled();
        return;
    }

    m_source = new SystemInfoSource(new SessionRemoteExec(m_session, this), this);
    connect(m_source, &SystemInfoSource::snapshotReady, this, &SystemInfoWidget::onSnapshotReady);
    connect(m_source,
            &SystemInfoSource::capabilityChanged,
            this,
            &SystemInfoWidget::onCapabilityChanged);
    connect(m_source, &SystemInfoSource::failed, this, &SystemInfoWidget::onSourceFailed);
    connect(m_session, &Session::stateChanged, this, &SystemInfoWidget::onSessionStateChanged);

    setStatus(tr("Checking…"), false);
    m_source->start();
}

SystemInfoWidget::~SystemInfoWidget()
{
    if (m_source) {
        m_source->stop();
    }
}

void SystemInfoWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing);
    root->setSpacing(UiMetrics::relatedSpacing);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->hide();
    root->addWidget(m_statusLabel);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildOverviewPage(), tr("Overview"));
    m_tabs->addTab(buildCpuMemPage(), tr("CPU & Memory"));
    m_tabs->addTab(buildDiskPage(), tr("Disk"));
    m_tabs->addTab(buildNetworkPage(), tr("Network"));
    m_tabs->addTab(buildGpuPage(), tr("GPU"));
    m_tabs->addTab(buildVirtPage(), tr("Virtualization"));
    root->addWidget(m_tabs, 1);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->addStretch(1);
    m_copyButton = new QPushButton(tr("Copy"), this);
    auto *copyMenu = new QMenu(m_copyButton);
    copyMenu->addAction(tr("Copy as text"), this, &SystemInfoWidget::copyAsText);
    copyMenu->addAction(tr("Copy as JSON"), this, &SystemInfoWidget::copyAsJson);
    m_copyButton->setMenu(copyMenu);
    updateCopyEnabled();
    toolbar->addWidget(m_copyButton);
    root->addLayout(toolbar);
}

QWidget *SystemInfoWidget::buildOverviewPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(UiMetrics::sectionSpacing);

    auto *identity = makeGroupFrame(tr("Identity"), page);
    auto *identityLayout = qobject_cast<QVBoxLayout *>(identity->layout());
    auto *form = new QFormLayout();
    form->setHorizontalSpacing(UiMetrics::sectionSpacing);
    form->setVerticalSpacing(UiMetrics::tightSpacing);
    m_osLabel = makeValueLabel(identity);
    m_kernelLabel = makeValueLabel(identity);
    m_archLabel = makeValueLabel(identity);
    m_hostnameLabel = makeValueLabel(identity);
    m_uptimeLabel = makeValueLabel(identity);
    form->addRow(tr("OS"), m_osLabel);
    form->addRow(tr("Kernel"), m_kernelLabel);
    form->addRow(tr("Architecture"), m_archLabel);
    form->addRow(tr("Hostname"), m_hostnameLabel);
    form->addRow(tr("Uptime"), m_uptimeLabel);
    identityLayout->addLayout(form);
    layout->addWidget(identity);

    auto *load = makeGroupFrame(tr("Load average"), page);
    auto *loadLayout = qobject_cast<QVBoxLayout *>(load->layout());
    m_loadLabel = makeValueLabel(load);
    loadLayout->addWidget(m_loadLabel);
    layout->addWidget(load);

    m_tempGroup = makeGroupFrame(tr("Temperature"), page);
    auto *tempsLayout = qobject_cast<QVBoxLayout *>(m_tempGroup->layout());
    m_tempTable = new QTableWidget(0, 2, m_tempGroup);
    configureTable(m_tempTable);
    m_tempTable->setHorizontalHeaderLabels({tr("Sensor"), tr("Temp")});
    m_tempTable->setMaximumHeight(160);
    tempsLayout->addWidget(m_tempTable);
    m_tempGroup->hide();
    layout->addWidget(m_tempGroup);

    layout->addStretch(1);
    return page;
}

QWidget *SystemInfoWidget::buildCpuMemPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(UiMetrics::sectionSpacing);

    auto *cpu = makeGroupFrame(tr("CPU"), page);
    auto *cpuLayout = qobject_cast<QVBoxLayout *>(cpu->layout());
    m_cpuModelLabel = makeValueLabel(cpu);
    m_cpuFreqLabel = makeValueLabel(cpu);
    m_cpuUsageLabel = makeValueLabel(cpu);
    m_cpuBar = new QProgressBar(cpu);
    m_cpuBar->setRange(0, 1000);
    m_cpuBar->setValue(0);
    m_cpuBar->setTextVisible(false);
    m_cpuBar->setMinimumHeight(12);
    cpuLayout->addWidget(m_cpuModelLabel);
    cpuLayout->addWidget(m_cpuFreqLabel);
    cpuLayout->addWidget(m_cpuUsageLabel);
    cpuLayout->addWidget(m_cpuBar);

    m_coreTable = new QTableWidget(0, 3, cpu);
    configureTable(m_coreTable);
    m_coreTable->setHorizontalHeaderLabels({tr("Core"), tr("Usage"), tr("Frequency")});
    m_coreTable->setMaximumHeight(200);
    cpuLayout->addWidget(m_coreTable);
    layout->addWidget(cpu);

    auto *mem = makeGroupFrame(tr("Memory"), page);
    auto *memLayout = qobject_cast<QVBoxLayout *>(mem->layout());
    m_memBreakdownLabel = makeValueLabel(mem);
    m_memSummaryLabel = makeValueLabel(mem);
    m_memBar = new QProgressBar(mem);
    m_memBar->setRange(0, 1000);
    m_memBar->setValue(0);
    m_memBar->setTextVisible(false);
    m_memBar->setMinimumHeight(12);
    m_swapSummaryLabel = makeValueLabel(mem);
    m_swapBar = new QProgressBar(mem);
    m_swapBar->setRange(0, 1000);
    m_swapBar->setValue(0);
    m_swapBar->setTextVisible(false);
    m_swapBar->setMinimumHeight(12);
    memLayout->addWidget(m_memBreakdownLabel);
    memLayout->addWidget(m_memSummaryLabel);
    memLayout->addWidget(m_memBar);
    memLayout->addWidget(m_swapSummaryLabel);
    memLayout->addWidget(m_swapBar);
    layout->addWidget(mem);

    layout->addStretch(1);
    return page;
}

QWidget *SystemInfoWidget::buildDiskPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(UiMetrics::sectionSpacing);

    m_diskMountsGroup = makeGroupFrame(tr("Mounts"), page);
    auto *mountsLayout = qobject_cast<QVBoxLayout *>(m_diskMountsGroup->layout());
    m_diskTable = new QTableWidget(0, 6, m_diskMountsGroup);
    configureTable(m_diskTable);
    m_diskTable->setHorizontalHeaderLabels(
        {tr("Filesystem"), tr("Mount"), tr("Size"), tr("Used"), tr("Avail"), tr("Use%")});
    mountsLayout->addWidget(m_diskTable);
    layout->addWidget(m_diskMountsGroup, 1);

    m_diskIoGroup = makeGroupFrame(tr("Disk I/O"), page);
    auto *ioLayout = qobject_cast<QVBoxLayout *>(m_diskIoGroup->layout());
    m_diskIoTable = new QTableWidget(0, 6, m_diskIoGroup);
    configureTable(m_diskIoTable);
    m_diskIoTable->setHorizontalHeaderLabels(
        {tr("Device"), tr("Read"), tr("Write"), tr("Read IOPS"), tr("Write IOPS"), tr("Util")});
    ioLayout->addWidget(m_diskIoTable);
    layout->addWidget(m_diskIoGroup, 1);
    return page;
}

QWidget *SystemInfoWidget::buildNetworkPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);

    m_nicTable = new QTableWidget(0, 8, page);
    configureTable(m_nicTable);
    m_nicTable->setHorizontalHeaderLabels({tr("Interface"),
                                           tr("State"),
                                           tr("IPv4"),
                                           tr("IPv6"),
                                           tr("Speed"),
                                           tr("MTU"),
                                           tr("RX"),
                                           tr("TX")});
    layout->addWidget(m_nicTable);
    return page;
}

QWidget *SystemInfoWidget::buildGpuPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(UiMetrics::sectionSpacing);

    m_gpuEmptyLabel =
        new QLabel(tr("No NVIDIA GPU detected (nvidia-smi unavailable or no devices)."), page);
    m_gpuEmptyLabel->setWordWrap(true);
    m_gpuEmptyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_gpuEmptyLabel);

    m_gpuDevicesGroup = makeGroupFrame(tr("Devices"), page);
    auto *devicesLayout = qobject_cast<QVBoxLayout *>(m_gpuDevicesGroup->layout());
    m_gpuTable = new QTableWidget(0, 9, m_gpuDevicesGroup);
    configureTable(m_gpuTable);
    m_gpuTable->setHorizontalHeaderLabels({tr("Index"),
                                           tr("Name"),
                                           tr("Util"),
                                           tr("Memory"),
                                           tr("Temp"),
                                           tr("Power"),
                                           tr("P-state"),
                                           tr("Clocks"),
                                           tr("Driver")});
    auto *utilHeader = m_gpuTable->horizontalHeaderItem(2);
    if (utilHeader) {
        utilHeader->setToolTip(
            tr("Percent of time over the sample period during which one or more kernels "
               "were executing on the GPU (not FLOP capacity)."));
    }
    devicesLayout->addWidget(m_gpuTable);
    m_gpuDevicesGroup->hide();
    layout->addWidget(m_gpuDevicesGroup, 1);

    m_gpuProcsGroup = makeGroupFrame(tr("Compute processes"), page);
    auto *procsLayout = qobject_cast<QVBoxLayout *>(m_gpuProcsGroup->layout());
    m_gpuProcTable = new QTableWidget(0, 4, m_gpuProcsGroup);
    configureTable(m_gpuProcTable);
    m_gpuProcTable->setHorizontalHeaderLabels(
        {tr("PID"), tr("Name"), tr("GPU UUID"), tr("Memory")});
    procsLayout->addWidget(m_gpuProcTable);
    m_gpuProcsGroup->hide();
    layout->addWidget(m_gpuProcsGroup, 1);

    return page;
}

QWidget *SystemInfoWidget::buildVirtPage()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing,
                               UiMetrics::relatedSpacing);
    layout->setSpacing(UiMetrics::sectionSpacing);

    auto *detection = makeGroupFrame(tr("Detection"), content);
    auto *detectionLayout = qobject_cast<QVBoxLayout *>(detection->layout());
    auto *detectForm = new QFormLayout();
    detectForm->setHorizontalSpacing(UiMetrics::sectionSpacing);
    detectForm->setVerticalSpacing(UiMetrics::tightSpacing);
    m_virtDetectLabel = makeValueLabel(detection);
    m_virtVmLabel = makeValueLabel(detection);
    m_virtContainerLabel = makeValueLabel(detection);
    m_virtIsVmLabel = makeValueLabel(detection);
    m_virtIsContainerLabel = makeValueLabel(detection);
    m_virtHypervisorFlagLabel = makeValueLabel(detection);
    m_virtCpuVendorLabel = makeValueLabel(detection);
    detectForm->addRow(tr("Detected"), m_virtDetectLabel);
    detectForm->addRow(tr("VM type"), m_virtVmLabel);
    detectForm->addRow(tr("Container type"), m_virtContainerLabel);
    detectForm->addRow(tr("Running as VM"), m_virtIsVmLabel);
    detectForm->addRow(tr("Running as container"), m_virtIsContainerLabel);
    detectForm->addRow(tr("CPU hypervisor flag"), m_virtHypervisorFlagLabel);
    detectForm->addRow(tr("CPU vendor"), m_virtCpuVendorLabel);
    detectionLayout->addLayout(detectForm);
    layout->addWidget(detection);

    auto *hints = makeGroupFrame(tr("Container / environment hints"), content);
    auto *hintsLayout = qobject_cast<QVBoxLayout *>(hints->layout());
    auto *hintsForm = new QFormLayout();
    hintsForm->setHorizontalSpacing(UiMetrics::sectionSpacing);
    hintsForm->setVerticalSpacing(UiMetrics::tightSpacing);
    m_virtDockerLabel = makeValueLabel(hints);
    m_virtPodmanLabel = makeValueLabel(hints);
    m_virtWslLabel = makeValueLabel(hints);
    m_virtCgroupLabel = makeValueLabel(hints);
    hintsForm->addRow(tr("Docker (.dockerenv)"), m_virtDockerLabel);
    hintsForm->addRow(tr("Podman (.containerenv)"), m_virtPodmanLabel);
    hintsForm->addRow(tr("WSL"), m_virtWslLabel);
    hintsForm->addRow(tr("Init cgroup"), m_virtCgroupLabel);
    hintsLayout->addLayout(hintsForm);
    layout->addWidget(hints);

    auto *dmi = makeGroupFrame(tr("DMI / firmware"), content);
    auto *dmiLayout = qobject_cast<QVBoxLayout *>(dmi->layout());
    auto *dmiForm = new QFormLayout();
    dmiForm->setHorizontalSpacing(UiMetrics::sectionSpacing);
    dmiForm->setVerticalSpacing(UiMetrics::tightSpacing);
    m_dmiSysVendorLabel = makeValueLabel(dmi);
    m_dmiProductLabel = makeValueLabel(dmi);
    m_dmiProductVerLabel = makeValueLabel(dmi);
    m_dmiBoardLabel = makeValueLabel(dmi);
    m_dmiChassisLabel = makeValueLabel(dmi);
    m_dmiBiosLabel = makeValueLabel(dmi);
    dmiForm->addRow(tr("System vendor"), m_dmiSysVendorLabel);
    dmiForm->addRow(tr("Product"), m_dmiProductLabel);
    dmiForm->addRow(tr("Product version"), m_dmiProductVerLabel);
    dmiForm->addRow(tr("Board"), m_dmiBoardLabel);
    dmiForm->addRow(tr("Chassis"), m_dmiChassisLabel);
    dmiForm->addRow(tr("BIOS"), m_dmiBiosLabel);
    dmiLayout->addLayout(dmiForm);
    layout->addWidget(dmi);

    layout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll);
    return page;
}

void SystemInfoWidget::configureTable(QTableWidget *table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);
}

void SystemInfoWidget::setLabel(QLabel *label, const QString &text)
{
    if (label) {
        label->setText(text.isEmpty() ? QStringLiteral("—") : text);
    }
}

void SystemInfoWidget::setStatus(const QString &text, bool errorStyle)
{
    if (!m_statusLabel) {
        return;
    }
    m_statusLabel->setText(text);
    QPalette labelPalette = m_statusLabel->palette();
    if (errorStyle) {
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        labelPalette.setColor(QPalette::WindowText,
                              dark ? QColor(QStringLiteral("#f0a0a0"))
                                   : QColor(QStringLiteral("#b00020")));
    } else {
        labelPalette.setColor(QPalette::WindowText, palette().color(QPalette::WindowText));
    }
    m_statusLabel->setPalette(labelPalette);
    m_statusLabel->setVisible(!text.isEmpty());
}

void SystemInfoWidget::clearStatus()
{
    if (!m_statusLabel) {
        return;
    }
    m_statusLabel->clear();
    QPalette labelPalette = m_statusLabel->palette();
    labelPalette.setColor(QPalette::WindowText, palette().color(QPalette::WindowText));
    m_statusLabel->setPalette(labelPalette);
    m_statusLabel->hide();
}

void SystemInfoWidget::updateCopyEnabled()
{
    if (m_copyButton) {
        m_copyButton->setEnabled(m_lastSnapshot.has_value());
    }
}

QString SystemInfoWidget::yesNo(bool value)
{
    return value ? QCoreApplication::translate("SystemInfoWidget", "Yes")
                 : QCoreApplication::translate("SystemInfoWidget", "No");
}

void SystemInfoWidget::applySnapshot(const SystemInfo &info)
{
    setLabel(m_osLabel, info.os.prettyName);
    setLabel(m_kernelLabel, info.os.kernel);
    setLabel(m_archLabel, info.os.arch);
    setLabel(m_hostnameLabel, info.os.hostname);
    setLabel(m_uptimeLabel, SystemInfoParser::formatUptime(info.os.uptimeSec));
    setLabel(m_loadLabel,
             tr("%1 / %2 / %3 (1 / 5 / 15 min)")
                 .arg(info.load.load1, 0, 'f', 2)
                 .arg(info.load.load5, 0, 'f', 2)
                 .arg(info.load.load15, 0, 'f', 2));

    m_tempTable->setRowCount(info.temps.size());
    if (info.temps.isEmpty()) {
        if (m_tempGroup) {
            m_tempGroup->hide();
        }
    } else {
        for (int i = 0; i < info.temps.size(); ++i) {
            const TempSensorInfo &t = info.temps.at(i);
            m_tempTable->setItem(i, 0, new QTableWidgetItem(t.name));
            m_tempTable->setItem(
                i, 1, new QTableWidgetItem(SystemInfoParser::formatCelsius(t.celsius)));
        }
        if (m_tempGroup) {
            m_tempGroup->show();
        }
    }

    const QString modelText =
        info.cpu.logicalCpus > 0
            ? tr("%1 (%2 logical)")
                  .arg(info.cpu.model.isEmpty() ? tr("Unknown") : info.cpu.model)
                  .arg(info.cpu.logicalCpus)
            : (info.cpu.model.isEmpty() ? QStringLiteral("—") : info.cpu.model);
    setLabel(m_cpuModelLabel, modelText);

    QString freqText;
    if (info.cpu.freqMinKHz > 0 || info.cpu.freqMaxKHz > 0) {
        freqText = tr("Governor: %1 · Range: %2 – %3")
                       .arg(info.cpu.governor.isEmpty() ? QStringLiteral("—") : info.cpu.governor,
                            SystemInfoParser::formatFreqKHz(info.cpu.freqMinKHz),
                            SystemInfoParser::formatFreqKHz(info.cpu.freqMaxKHz));
    } else if (!info.cpu.governor.isEmpty()) {
        freqText = tr("Governor: %1").arg(info.cpu.governor);
    }
    setLabel(m_cpuFreqLabel, freqText);
    if (m_cpuFreqLabel) {
        m_cpuFreqLabel->setVisible(!freqText.isEmpty());
    }

    setLabel(m_cpuUsageLabel,
             tr("Usage: %1").arg(SystemInfoParser::formatPercent(info.cpu.usagePercent)));
    if (info.cpu.usagePercent >= 0.0) {
        m_cpuBar->setValue(static_cast<int>(std::lround(info.cpu.usagePercent * 10.0)));
    } else {
        m_cpuBar->setValue(0);
    }

    m_coreTable->setRowCount(info.cpu.cores.size());
    for (int i = 0; i < info.cpu.cores.size(); ++i) {
        const double pct =
            i < info.cpu.coreUsagePercent.size() ? info.cpu.coreUsagePercent.at(i) : -1.0;
        const qint64 freq = i < info.cpu.coreFreqKHz.size() ? info.cpu.coreFreqKHz.at(i) : 0;
        m_coreTable->setItem(i, 0, new QTableWidgetItem(tr("cpu%1").arg(i)));
        m_coreTable->setItem(i, 1, new QTableWidgetItem(SystemInfoParser::formatPercent(pct)));
        m_coreTable->setItem(i, 2, new QTableWidgetItem(SystemInfoParser::formatFreqKHz(freq)));
    }
    if (m_coreTable) {
        m_coreTable->setVisible(!info.cpu.cores.isEmpty());
    }

    const quint64 usedKb = memUsedKb(info.mem);
    const quint64 availKb = info.mem.availableKb > 0
                                ? info.mem.availableKb
                                : (info.mem.freeKb + info.mem.buffersKb + info.mem.cachedKb);
    setLabel(m_memBreakdownLabel,
             tr("Buffers %1 · Cached %2 · Shmem %3 · SReclaimable %4")
                 .arg(SystemInfoParser::formatBytesFromKiB(info.mem.buffersKb),
                      SystemInfoParser::formatBytesFromKiB(info.mem.cachedKb),
                      SystemInfoParser::formatBytesFromKiB(info.mem.shmemKb),
                      SystemInfoParser::formatBytesFromKiB(info.mem.sReclaimableKb)));
    setLabel(m_memSummaryLabel,
             tr("Used %1 / %2 (available %3)")
                 .arg(SystemInfoParser::formatBytesFromKiB(usedKb),
                      SystemInfoParser::formatBytesFromKiB(info.mem.totalKb),
                      SystemInfoParser::formatBytesFromKiB(availKb)));
    if (info.mem.totalKb > 0) {
        m_memBar->setValue(static_cast<int>(std::lround(1000.0 * static_cast<double>(usedKb) /
                                                        static_cast<double>(info.mem.totalKb))));
    } else {
        m_memBar->setValue(0);
    }

    const quint64 swapUsed =
        info.mem.swapTotalKb > info.mem.swapFreeKb ? info.mem.swapTotalKb - info.mem.swapFreeKb : 0;
    const bool hasSwap = info.mem.swapTotalKb > 0;
    if (hasSwap) {
        setLabel(m_swapSummaryLabel,
                 tr("Swap used %1 / %2")
                     .arg(SystemInfoParser::formatBytesFromKiB(swapUsed),
                          SystemInfoParser::formatBytesFromKiB(info.mem.swapTotalKb)));
        m_swapBar->setValue(static_cast<int>(std::lround(
            1000.0 * static_cast<double>(swapUsed) / static_cast<double>(info.mem.swapTotalKb))));
    }
    if (m_swapSummaryLabel) {
        m_swapSummaryLabel->setVisible(hasSwap);
    }
    if (m_swapBar) {
        m_swapBar->setVisible(hasSwap);
    }

    m_diskTable->setRowCount(info.disks.size());
    for (int i = 0; i < info.disks.size(); ++i) {
        const DiskInfo &d = info.disks.at(i);
        m_diskTable->setItem(i, 0, new QTableWidgetItem(d.filesystem));
        m_diskTable->setItem(i, 1, new QTableWidgetItem(d.mountpoint));
        m_diskTable->setItem(
            i, 2, new QTableWidgetItem(SystemInfoParser::formatBytesFromKiB(d.sizeKb)));
        m_diskTable->setItem(
            i, 3, new QTableWidgetItem(SystemInfoParser::formatBytesFromKiB(d.usedKb)));
        m_diskTable->setItem(
            i, 4, new QTableWidgetItem(SystemInfoParser::formatBytesFromKiB(d.availKb)));
        m_diskTable->setItem(i, 5, new QTableWidgetItem(tr("%1%").arg(d.usePercent)));
    }
    if (m_diskMountsGroup) {
        m_diskMountsGroup->setVisible(!info.disks.isEmpty());
    }

    m_diskIoTable->setRowCount(info.diskIo.size());
    for (int i = 0; i < info.diskIo.size(); ++i) {
        const DiskIoInfo &d = info.diskIo.at(i);
        m_diskIoTable->setItem(i, 0, new QTableWidgetItem(d.name));
        m_diskIoTable->setItem(
            i, 1, new QTableWidgetItem(SystemInfoParser::formatRateBps(d.readBps)));
        m_diskIoTable->setItem(
            i, 2, new QTableWidgetItem(SystemInfoParser::formatRateBps(d.writeBps)));
        m_diskIoTable->setItem(
            i, 3, new QTableWidgetItem(SystemInfoParser::formatIops(d.readIops)));
        m_diskIoTable->setItem(
            i, 4, new QTableWidgetItem(SystemInfoParser::formatIops(d.writeIops)));
        m_diskIoTable->setItem(
            i, 5, new QTableWidgetItem(SystemInfoParser::formatPercent(d.utilPercent)));
    }
    if (m_diskIoGroup) {
        m_diskIoGroup->setVisible(!info.diskIo.isEmpty());
    }

    m_nicTable->setRowCount(info.nics.size());
    for (int i = 0; i < info.nics.size(); ++i) {
        const NicInfo &n = info.nics.at(i);
        const QString rxText = n.rxBps >= 0.0 ? SystemInfoParser::formatRateBps(n.rxBps)
                                              : SystemInfoParser::formatBytes(n.rxBytes);
        const QString txText = n.txBps >= 0.0 ? SystemInfoParser::formatRateBps(n.txBps)
                                              : SystemInfoParser::formatBytes(n.txBytes);
        m_nicTable->setItem(i, 0, new QTableWidgetItem(n.name));
        m_nicTable->setItem(
            i, 1, new QTableWidgetItem(n.operState.isEmpty() ? QStringLiteral("—") : n.operState));
        m_nicTable->setItem(
            i, 2, new QTableWidgetItem(n.ipv4.isEmpty() ? QStringLiteral("—") : n.ipv4));
        m_nicTable->setItem(
            i, 3, new QTableWidgetItem(n.ipv6.isEmpty() ? QStringLiteral("—") : n.ipv6));
        m_nicTable->setItem(
            i, 4, new QTableWidgetItem(SystemInfoParser::formatLinkSpeed(n.speedMbps)));
        m_nicTable->setItem(
            i, 5, new QTableWidgetItem(n.mtu > 0 ? QString::number(n.mtu) : QStringLiteral("—")));
        m_nicTable->setItem(i, 6, new QTableWidgetItem(rxText));
        m_nicTable->setItem(i, 7, new QTableWidgetItem(txText));
    }
    if (m_nicTable) {
        m_nicTable->setVisible(!info.nics.isEmpty());
    }

    const bool hasGpus = !info.gpus.isEmpty();
    if (m_gpuEmptyLabel) {
        m_gpuEmptyLabel->setVisible(!hasGpus);
    }
    if (m_gpuTable) {
        m_gpuTable->setRowCount(info.gpus.size());
        for (int i = 0; i < info.gpus.size(); ++i) {
            const GpuInfo &g = info.gpus.at(i);
            const QString mem = (g.memUsedMiB >= 0 && g.memTotalMiB >= 0)
                                    ? tr("%1 / %2 MiB").arg(g.memUsedMiB).arg(g.memTotalMiB)
                                    : QStringLiteral("—");
            QString power = QStringLiteral("—");
            if (g.powerDrawW >= 0.0) {
                power =
                    g.powerLimitW >= 0.0
                        ? tr("%1 / %2 W").arg(g.powerDrawW, 0, 'f', 1).arg(g.powerLimitW, 0, 'f', 0)
                        : tr("%1 W").arg(g.powerDrawW, 0, 'f', 1);
            }
            QString clocks = QStringLiteral("—");
            if (g.clockSmMHz >= 0 || g.clockMemMHz >= 0) {
                clocks = tr("%1 / %2 MHz")
                             .arg(g.clockSmMHz >= 0 ? QString::number(g.clockSmMHz)
                                                    : QStringLiteral("—"),
                                  g.clockMemMHz >= 0 ? QString::number(g.clockMemMHz)
                                                     : QStringLiteral("—"));
            }

            auto *indexItem =
                new QTableWidgetItem(g.index >= 0 ? QString::number(g.index) : QStringLiteral("—"));
            auto *nameItem = new QTableWidgetItem(g.name.isEmpty() ? QStringLiteral("—") : g.name);
            if (!g.uuid.isEmpty()) {
                nameItem->setToolTip(g.uuid);
            }
            auto *utilItem =
                new QTableWidgetItem(SystemInfoParser::formatPercent(g.utilGpuPercent));
            utilItem->setToolTip(
                tr("Percent of time over the sample period during which one or more kernels "
                   "were executing on the GPU (not FLOP capacity)."));

            m_gpuTable->setItem(i, 0, indexItem);
            m_gpuTable->setItem(i, 1, nameItem);
            m_gpuTable->setItem(i, 2, utilItem);
            m_gpuTable->setItem(i, 3, new QTableWidgetItem(mem));
            m_gpuTable->setItem(
                i, 4, new QTableWidgetItem(SystemInfoParser::formatCelsius(g.tempCelsius)));
            m_gpuTable->setItem(i, 5, new QTableWidgetItem(power));
            m_gpuTable->setItem(
                i, 6, new QTableWidgetItem(g.pstate.isEmpty() ? QStringLiteral("—") : g.pstate));
            m_gpuTable->setItem(i, 7, new QTableWidgetItem(clocks));
            m_gpuTable->setItem(i,
                                8,
                                new QTableWidgetItem(g.driverVersion.isEmpty() ? QStringLiteral("—")
                                                                               : g.driverVersion));
        }
    }
    if (m_gpuDevicesGroup) {
        m_gpuDevicesGroup->setVisible(hasGpus);
    }

    if (m_gpuProcTable) {
        m_gpuProcTable->setRowCount(info.gpuProcesses.size());
        for (int i = 0; i < info.gpuProcesses.size(); ++i) {
            const GpuProcessInfo &p = info.gpuProcesses.at(i);
            QString uuidShort = p.gpuUuid;
            if (uuidShort.size() > 13) {
                uuidShort = uuidShort.left(13) + QLatin1String("…");
            }
            auto *uuidItem =
                new QTableWidgetItem(uuidShort.isEmpty() ? QStringLiteral("—") : uuidShort);
            if (!p.gpuUuid.isEmpty()) {
                uuidItem->setToolTip(p.gpuUuid);
            }
            m_gpuProcTable->setItem(
                i,
                0,
                new QTableWidgetItem(p.pid >= 0 ? QString::number(p.pid) : QStringLiteral("—")));
            m_gpuProcTable->setItem(
                i, 1, new QTableWidgetItem(p.name.isEmpty() ? QStringLiteral("—") : p.name));
            m_gpuProcTable->setItem(i, 2, uuidItem);
            m_gpuProcTable->setItem(i,
                                    3,
                                    new QTableWidgetItem(p.usedMemoryMiB >= 0
                                                             ? tr("%1 MiB").arg(p.usedMemoryMiB)
                                                             : QStringLiteral("—")));
        }
    }
    if (m_gpuProcsGroup) {
        m_gpuProcsGroup->setVisible(hasGpus && !info.gpuProcesses.isEmpty());
    }

    const VirtInfo &v = info.virt;
    setLabel(m_virtDetectLabel, v.detectVirt);
    setLabel(m_virtVmLabel, v.vm);
    setLabel(m_virtContainerLabel, v.container);
    setLabel(m_virtIsVmLabel, yesNo(v.isVm));
    setLabel(m_virtIsContainerLabel, yesNo(v.isContainer));
    setLabel(m_virtHypervisorFlagLabel, yesNo(v.cpuHypervisorFlag));
    setLabel(m_virtCpuVendorLabel, v.cpuVendor);
    setLabel(m_virtDockerLabel, yesNo(v.dockerEnv));
    setLabel(m_virtPodmanLabel, yesNo(v.podmanEnv));
    setLabel(m_virtWslLabel, yesNo(v.wsl));
    setLabel(m_virtCgroupLabel, v.cgroupInit);
    setLabel(m_dmiSysVendorLabel, v.dmiSysVendor);
    setLabel(m_dmiProductLabel, v.dmiProductName);
    setLabel(m_dmiProductVerLabel, v.dmiProductVersion);
    const QString board = (v.dmiBoardVendor.isEmpty() && v.dmiBoardName.isEmpty())
                              ? QString()
                              : tr("%1 %2").arg(v.dmiBoardVendor, v.dmiBoardName).trimmed();
    setLabel(m_dmiBoardLabel, board);
    const QString chassis =
        (v.dmiChassisVendor.isEmpty() && v.dmiChassisType.isEmpty())
            ? QString()
            : tr("%1 (type %2)")
                  .arg(v.dmiChassisVendor.isEmpty() ? QStringLiteral("—") : v.dmiChassisVendor,
                       v.dmiChassisType.isEmpty() ? QStringLiteral("—") : v.dmiChassisType);
    setLabel(m_dmiChassisLabel, chassis);
    const QString bios =
        (v.dmiBiosVendor.isEmpty() && v.dmiBiosVersion.isEmpty() && v.dmiBiosDate.isEmpty())
            ? QString()
            : tr("%1 %2 (%3)")
                  .arg(v.dmiBiosVendor.isEmpty() ? QStringLiteral("—") : v.dmiBiosVendor,
                       v.dmiBiosVersion.isEmpty() ? QStringLiteral("—") : v.dmiBiosVersion,
                       v.dmiBiosDate.isEmpty() ? QStringLiteral("—") : v.dmiBiosDate);
    setLabel(m_dmiBiosLabel, bios);
}

void SystemInfoWidget::onSnapshotReady(const SystemInfo &info)
{
    m_lastSnapshot = info;
    updateCopyEnabled();
    clearStatus();
    m_tabs->setEnabled(true);
    applySnapshot(info);
}

void SystemInfoWidget::onCapabilityChanged(ExplorerCapability capability)
{
    if (!m_source) {
        return;
    }

    const QString message = m_source->capabilityMessage();
    switch (capability) {
    case ExplorerCapability::Checking:
        setStatus(message.isEmpty() ? tr("Checking…") : message, false);
        break;
    case ExplorerCapability::Available:
        clearStatus();
        m_tabs->setEnabled(true);
        break;
    case ExplorerCapability::Unavailable:
    case ExplorerCapability::PermissionDenied:
    case ExplorerCapability::Error:
        setStatus(message.isEmpty() ? tr("System info unavailable.") : message, true);
        m_tabs->setEnabled(false);
        break;
    }
}

void SystemInfoWidget::onSourceFailed(const QString &message)
{
    setStatus(message.isEmpty() ? tr("Failed to load system info.") : message, true);
}

void SystemInfoWidget::onSessionStateChanged()
{
    if (!m_session || m_session->state() != SessionState::Connected) {
        if (m_source) {
            m_source->stop();
        }
        m_tabs->setEnabled(false);
        setStatus(tr("Session disconnected."), true);
        m_lastSnapshot.reset();
        updateCopyEnabled();
    }
}

void SystemInfoWidget::copyAsText()
{
    if (!m_lastSnapshot.has_value()) {
        return;
    }
    if (QClipboard *clipboard = QApplication::clipboard()) {
        clipboard->setText(SystemInfoParser::formatSnapshotText(*m_lastSnapshot));
    }
}

void SystemInfoWidget::copyAsJson()
{
    if (!m_lastSnapshot.has_value()) {
        return;
    }
    if (QClipboard *clipboard = QApplication::clipboard()) {
        clipboard->setText(SystemInfoParser::formatSnapshotJson(*m_lastSnapshot));
    }
}
