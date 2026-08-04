// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ContainerSource.h"

#include "core/explorer/container/ContainerParser.h"
#include "core/session/Session.h"

#include <QTimer>
#include <QUuid>

ContainerSource::ContainerSource(Session *session, QObject *parent)
    : IExplorerSource(parent), m_session(session)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_pollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &ContainerSource::onPollTick);

    if (m_session) {
        connect(m_session, &Session::commandFinished, this, &ContainerSource::onCommandFinished);
    }
}

ContainerSource::~ContainerSource()
{
    stop();
}

void ContainerSource::setPollIntervalMs(int ms)
{
    m_pollIntervalMs = qMax(500, ms);
    m_timer->setInterval(m_pollIntervalMs);
}

void ContainerSource::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    setCapability(ExplorerCapability::Checking, tr("Checking…"));
    requestList();
    m_timer->start();
}

void ContainerSource::stop()
{
    m_running = false;
    m_timer->stop();
    m_activeRequestId.clear();
    m_requestInFlight = false;
    m_refreshAfterInFlight = false;
    setBusy(false);
}

void ContainerSource::refresh()
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

void ContainerSource::onPollTick()
{
    if (!m_running || m_requestInFlight) {
        return;
    }
    requestList();
}

void ContainerSource::requestList()
{
    if (!m_session) {
        setCapability(ExplorerCapability::Error, tr("No session"));
        setBusy(false);
        return;
    }

    m_activeRequestId = makeRequestId();
    m_requestInFlight = true;
    setBusy(true);
    m_session->execCommand(m_activeRequestId, ContainerParser::listCommand());
}

void ContainerSource::onCommandFinished(const QString &requestId,
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
            ContainerParser::classifyFailure(exitStatus, stderrBytes, errorMessage, &message);
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

    QVector<ContainerInfo> containers;
    QString parseError;
    if (!ContainerParser::parseList(stdoutBytes, &containers, &parseError)) {
        setCapability(ExplorerCapability::Error,
                      parseError.isEmpty() ? tr("Failed to parse container list") : parseError);
        emit failed(m_capabilityMessage);
        return;
    }

    setCapability(ExplorerCapability::Available);
    emit snapshotReady(containers);

    if (m_refreshAfterInFlight) {
        m_refreshAfterInFlight = false;
        requestList();
    }
}

void ContainerSource::setCapability(ExplorerCapability capability, const QString &message)
{
    const bool changed = m_capability != capability || m_capabilityMessage != message;
    m_capability = capability;
    m_capabilityMessage = message;
    if (changed) {
        emit capabilityChanged(capability);
    }
}

void ContainerSource::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(busy);
}

QString ContainerSource::makeRequestId()
{
    return QStringLiteral("container-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}
