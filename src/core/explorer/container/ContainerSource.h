/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/IExplorerSource.h"
#include "core/explorer/container/ContainerInfo.h"

#include <QPointer>
#include <QUuid>
#include <QVector>

class QTimer;
class Session;

/// Polls remote container engines via Session::execCommand and emits typed snapshots.
class ContainerSource final : public IExplorerSource
{
    Q_OBJECT

public:
    static constexpr int kDefaultPollIntervalMs = 2000;

    explicit ContainerSource(Session *session, QObject *parent = nullptr);
    ~ContainerSource() override;

    void start() override;
    void stop() override;
    void refresh() override;

    ExplorerUpdateMode updateMode() const override { return ExplorerUpdateMode::Snapshot; }
    ExplorerCapability capability() const override { return m_capability; }
    QString capabilityMessage() const override { return m_capabilityMessage; }

    void setPollIntervalMs(int ms);
    int pollIntervalMs() const { return m_pollIntervalMs; }

signals:
    void snapshotReady(const QVector<ContainerInfo> &containers);

private slots:
    void onPollTick();
    void onCommandFinished(const QString &requestId,
                           int exitStatus,
                           const QByteArray &stdoutBytes,
                           const QByteArray &stderrBytes,
                           const QString &errorMessage);

private:
    void setCapability(ExplorerCapability capability, const QString &message = {});
    void setBusy(bool busy);
    void requestList();
    QString makeRequestId();

    QPointer<Session> m_session;
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
