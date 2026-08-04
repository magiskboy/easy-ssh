/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/ExplorerTypes.h"
#include "core/explorer/systeminfo/SystemInfo.h"

#include <QDialog>
#include <QPointer>
#include <optional>

class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QTableWidget;
class QTabWidget;
class Session;
class SystemInfoSource;

/// Modeless dialog showing remote host metrics in related tabs.
class SystemInfoDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SystemInfoDialog(Session *session, QWidget *parent = nullptr);
    ~SystemInfoDialog() override;

private slots:
    void onSnapshotReady(const SystemInfo &info);
    void onCapabilityChanged(ExplorerCapability capability);
    void onSessionStateChanged();
    void onSourceFailed(const QString &message);
    void copyAsText();
    void copyAsJson();

private:
    void buildUi();
    QWidget *buildOverviewPage();
    QWidget *buildCpuMemPage();
    QWidget *buildDiskPage();
    QWidget *buildNetworkPage();
    QWidget *buildGpuPage();
    QWidget *buildVirtPage();
    void applySnapshot(const SystemInfo &info);
    void setLabel(QLabel *label, const QString &text);
    void setStatus(const QString &text, bool errorStyle);
    void clearStatus();
    void updateCopyEnabled();
    static void configureTable(QTableWidget *table);
    static QString yesNo(bool value);

    QPointer<Session> m_session;
    SystemInfoSource *m_source = nullptr;
    std::optional<SystemInfo> m_lastSnapshot;

    QLabel *m_statusLabel = nullptr;
    QTabWidget *m_tabs = nullptr;
    QPushButton *m_copyButton = nullptr;

    // Overview
    QLabel *m_osLabel = nullptr;
    QLabel *m_kernelLabel = nullptr;
    QLabel *m_archLabel = nullptr;
    QLabel *m_hostnameLabel = nullptr;
    QLabel *m_uptimeLabel = nullptr;
    QLabel *m_loadLabel = nullptr;
    QWidget *m_tempGroup = nullptr;
    QTableWidget *m_tempTable = nullptr;

    // CPU & Memory
    QLabel *m_cpuModelLabel = nullptr;
    QLabel *m_cpuFreqLabel = nullptr;
    QLabel *m_cpuUsageLabel = nullptr;
    QProgressBar *m_cpuBar = nullptr;
    QTableWidget *m_coreTable = nullptr;
    QLabel *m_memBreakdownLabel = nullptr;
    QLabel *m_memSummaryLabel = nullptr;
    QProgressBar *m_memBar = nullptr;
    QLabel *m_swapSummaryLabel = nullptr;
    QProgressBar *m_swapBar = nullptr;

    // Disk / Network
    QWidget *m_diskMountsGroup = nullptr;
    QWidget *m_diskIoGroup = nullptr;
    QTableWidget *m_diskTable = nullptr;
    QTableWidget *m_diskIoTable = nullptr;
    QTableWidget *m_nicTable = nullptr;

    // GPU
    QLabel *m_gpuEmptyLabel = nullptr;
    QWidget *m_gpuDevicesGroup = nullptr;
    QWidget *m_gpuProcsGroup = nullptr;
    QTableWidget *m_gpuTable = nullptr;
    QTableWidget *m_gpuProcTable = nullptr;

    // Virtualization
    QLabel *m_virtDetectLabel = nullptr;
    QLabel *m_virtVmLabel = nullptr;
    QLabel *m_virtContainerLabel = nullptr;
    QLabel *m_virtIsVmLabel = nullptr;
    QLabel *m_virtIsContainerLabel = nullptr;
    QLabel *m_virtHypervisorFlagLabel = nullptr;
    QLabel *m_virtCpuVendorLabel = nullptr;
    QLabel *m_virtDockerLabel = nullptr;
    QLabel *m_virtPodmanLabel = nullptr;
    QLabel *m_virtWslLabel = nullptr;
    QLabel *m_virtCgroupLabel = nullptr;
    QLabel *m_dmiSysVendorLabel = nullptr;
    QLabel *m_dmiProductLabel = nullptr;
    QLabel *m_dmiProductVerLabel = nullptr;
    QLabel *m_dmiBoardLabel = nullptr;
    QLabel *m_dmiChassisLabel = nullptr;
    QLabel *m_dmiBiosLabel = nullptr;
};
