// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ServiceSource.h"

#include "core/explorer/service/ServiceParser.h"
#include "core/explorer/IRemoteExec.h"

#include <QTimer>
#include <QUuid>

ServiceSource::ServiceSource(IRemoteExec *exec, QObject *parent)
    : IExplorerSource(parent), m_exec(exec)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_pollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &ServiceSource::onPollTick);

    if (m_exec) {
        connect(m_exec,
                &IRemoteExec::commandFinished,
                this,
                [this](const QString &requestId,
                       int exitStatus,
                       const QByteArray &stdoutBytes,
                       const QByteArray &stderrBytes,
                       const QString &errorMessage) {
                    onCommandFinished(requestId,
                                      exitStatus,
                                      CommandStreams{stdoutBytes, stderrBytes},
                                      errorMessage);
                });
    }
}

ServiceSource::~ServiceSource()
{
    stop();
}

void ServiceSource::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qMax(500, ms);
    m_timer->setInterval(m_pollIntervalMs);
}

void ServiceSource::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    setCapability(ExplorerCapability::Checking, tr("Checking…"));
    requestList();
    m_timer->start();
}

void ServiceSource::stop()
{
    m_running = false;
    m_timer->stop();
    m_activeRequestId.clear();
    m_requestInFlight = false;
    m_refreshAfterInFlight = false;
    setBusy(false);
}

void ServiceSource::refresh()
{
    if (!m_running) {
        start();
        return;
    }
    if (m_requestInFlight) {
        m_refreshAfterInFlight = true;
        return;
    }
    requestList();
}

void ServiceSource::onPollTick()
{
    if (!m_running || m_requestInFlight) {
        return;
    }
    requestList();
}

void ServiceSource::requestList()
{
    if (!m_exec) {
        setCapability(ExplorerCapability::Error, tr("No session"));
        setBusy(false);
        return;
    }

    m_activeRequestId = makeRequestId();
    m_requestInFlight = true;
    setBusy(true);
    m_exec->execCommand(m_activeRequestId, ServiceParser::listCommand());
}

void ServiceSource::onCommandFinished(const QString &requestId,
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
        const ExplorerCapability cap =
            ServiceParser::classifyFailure(exitStatus, streams.stderrBytes, errorMessage, &message);
        setCapability(cap, message);
        if (cap == ExplorerCapability::Error) {
            emit failed(message);
        }
        if (m_refreshAfterInFlight) {
            m_refreshAfterInFlight = false;
            requestList();
        }
        return;
    }

    QVector<ServiceInfo> services;
    QString parseError;
    if (!ServiceParser::parseList(streams.stdoutBytes, &services, &parseError)) {
        setCapability(ExplorerCapability::Error,
                      parseError.isEmpty() ? tr("Failed to parse service list") : parseError);
        emit failed(m_capabilityMessage);
        return;
    }

    setCapability(ExplorerCapability::Available);
    emit snapshotReady(services);

    if (m_refreshAfterInFlight) {
        m_refreshAfterInFlight = false;
        requestList();
    }
}

void ServiceSource::setCapability(ExplorerCapability capability, const QString &message)
{
    const bool changed = m_capability != capability || m_capabilityMessage != message;
    m_capability = capability;
    m_capabilityMessage = message;
    if (changed) {
        emit capabilityChanged(capability);
    }
}

void ServiceSource::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(busy);
}

QString ServiceSource::makeRequestId()
{
    return QStringLiteral("service-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}
