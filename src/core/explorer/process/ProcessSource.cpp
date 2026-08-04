// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ProcessSource.h"

#include "core/explorer/process/ProcessParser.h"
#include "core/session/Session.h"

#include <QTimer>
#include <QUuid>

ProcessSource::ProcessSource(Session *session, QObject *parent)
    : IExplorerSource(parent), m_session(session)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_pollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &ProcessSource::onPollTick);

    if (m_session) {
        connect(m_session, &Session::commandFinished, this, &ProcessSource::onCommandFinished);
    }
}

ProcessSource::~ProcessSource()
{
    stop();
}

void ProcessSource::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qMax(500, ms);
    m_timer->setInterval(m_pollIntervalMs);
}

void ProcessSource::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    setCapability(ExplorerCapability::Checking, tr("Checking…"));
    requestList();
    m_timer->start();
}

void ProcessSource::stop()
{
    m_running = false;
    m_timer->stop();
    m_activeRequestId.clear();
    m_requestInFlight = false;
    m_refreshAfterInFlight = false;
    setBusy(false);
}

void ProcessSource::refresh()
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

void ProcessSource::onPollTick()
{
    if (!m_running || m_requestInFlight) {
        return;
    }
    requestList();
}

void ProcessSource::requestList()
{
    if (!m_session) {
        setCapability(ExplorerCapability::Error, tr("No session"));
        setBusy(false);
        return;
    }

    m_activeRequestId = makeRequestId();
    m_requestInFlight = true;
    setBusy(true);
    m_session->execCommand(m_activeRequestId, ProcessParser::listCommand());
}

void ProcessSource::onCommandFinished(const QString &requestId,
                                      int exitStatus,
                                      const QByteArray &stdoutBytes,
                                      const QByteArray &stderrBytes,
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
            ProcessParser::classifyFailure(exitStatus, stderrBytes, errorMessage, &message);
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

    QVector<ProcessInfo> processes;
    QString parseError;
    if (!ProcessParser::parsePsList(stdoutBytes, &processes, &parseError)) {
        setCapability(ExplorerCapability::Error,
                      parseError.isEmpty() ? tr("Failed to parse process list") : parseError);
        emit failed(m_capabilityMessage);
        return;
    }

    setCapability(ExplorerCapability::Available);
    emit snapshotReady(processes);

    if (m_refreshAfterInFlight) {
        m_refreshAfterInFlight = false;
        requestList();
    }
}

void ProcessSource::setCapability(ExplorerCapability capability, const QString &message)
{
    const bool changed = m_capability != capability || m_capabilityMessage != message;
    m_capability = capability;
    m_capabilityMessage = message;
    if (changed) {
        emit capabilityChanged(capability);
    }
}

void ProcessSource::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(busy);
}

QString ProcessSource::makeRequestId()
{
    return QStringLiteral("process-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}
