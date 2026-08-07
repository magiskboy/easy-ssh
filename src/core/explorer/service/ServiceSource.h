/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/IExplorerSource.h"
#include "core/explorer/IRemoteExec.h"
#include "core/explorer/service/ServiceInfo.h"

#include <QPointer>
#include <QVector>

class QTimer;

/// Polls remote service managers via IRemoteExec and emits typed snapshots.
class ServiceSource final : public IExplorerSource
{
    Q_OBJECT

public:
    static constexpr int kDefaultPollIntervalMs = 2000;

    explicit ServiceSource(IRemoteExec *exec, QObject *parent = nullptr);
    ~ServiceSource() override;

    void start() override;
    void stop() override;
    void refresh() override;

    ExplorerUpdateMode updateMode() const override { return ExplorerUpdateMode::Snapshot; }
    ExplorerCapability capability() const override { return m_capability; }
    QString capabilityMessage() const override { return m_capabilityMessage; }

    void setPollIntervalMs(int ms);
    int pollIntervalMs() const { return m_pollIntervalMs; }

signals:
    void snapshotReady(const QVector<ServiceInfo> &services);

private slots:
    void onPollTick();

private:
    struct CommandStreams
    {
        QByteArray stdoutBytes;
        QByteArray stderrBytes;
    };

    void onCommandFinished(const QString &requestId,
                           int exitStatus,
                           const CommandStreams &streams,
                           const QString &errorMessage);
    void setCapability(ExplorerCapability capability, const QString &message = {});
    void setBusy(bool busy);
    void requestList();
    QString makeRequestId();

    QPointer<IRemoteExec> m_exec;
    QTimer *m_timer = nullptr;
    ExplorerCapability m_capability = ExplorerCapability::Checking;
    QString m_capabilityMessage;
    bool m_busy = false;
    bool m_running = false;
    bool m_requestInFlight = false;
    bool m_refreshAfterInFlight = false;
    int m_pollIntervalMs = kDefaultPollIntervalMs;
    QString m_activeRequestId;
};
