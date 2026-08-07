// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ESSRemoteExecHost.h"

#include <QMetaObject>

#include <dispatch/dispatch.h>

ESSRemoteExecHost::ESSRemoteExecHost(QObject *parent) : IRemoteExec(parent) {}

void ESSRemoteExecHost::setWorker(SshWorker *worker)
{
    if (m_worker == worker) {
        return;
    }
    unwireWorker();
    m_worker = worker;
    m_connected = false;
    wireWorker();
}

void ESSRemoteExecHost::clearWorker()
{
    unwireWorker();
    m_worker = nullptr;
    m_connected = false;
}

void ESSRemoteExecHost::wireWorker()
{
    if (m_finishedConnection || m_worker == nullptr) {
        return;
    }
    // Bridge has QCoreApplication but no exec()/event pump on the main thread.
    // Qt QueuedConnection to main-thread QObjects never runs — mirror SFTP and hop via GCD.
    m_finishedConnection = QObject::connect(
        m_worker,
        &SshWorker::commandFinished,
        m_worker,
        // By-value params: GCD blocks capture C++ refs as dangling; owned copies outlive the hop.
        [this](QString requestId,
               int exitStatus,
               QByteArray stdoutBytes, // NOLINT(bugprone-easily-swappable-parameters)
               QByteArray stderrBytes, // NOLINT(bugprone-easily-swappable-parameters)
               QString errorMessage) {
            QPointer<ESSRemoteExecHost> self(this);
            dispatch_async(dispatch_get_main_queue(), ^{
              if (!self) {
                  return;
              }
              emit self->commandFinished(
                  requestId, exitStatus, stdoutBytes, stderrBytes, errorMessage);
            });
        });
}

void ESSRemoteExecHost::unwireWorker()
{
    if (m_finishedConnection) {
        QObject::disconnect(m_finishedConnection);
        m_finishedConnection = {};
    }
}

void ESSRemoteExecHost::execCommand(const QString &requestId, const QString &command)
{
    if (!m_connected || m_worker == nullptr) {
        emit commandFinished(requestId, -1, {}, {}, tr("SSH session is not connected"));
        return;
    }
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker.data(), requestId, command]() {
            if (worker != nullptr) {
                worker->execCommand(requestId, command);
            }
        },
        Qt::QueuedConnection);
}
