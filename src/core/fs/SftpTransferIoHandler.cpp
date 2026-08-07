// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/fs/SftpTransferIoHandler.h"

#include "core/ssh/SshIoLoop.h"

SftpTransferIoHandler::SftpTransferIoHandler(FsRemote *fs, Request request, Hooks hooks)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_fs(fs),
      m_request(std::move(request)), m_hooks(std::move(hooks))
{
}

SftpTransferIoHandler::~SftpTransferIoHandler()
{
    cancel();
}

QString SftpTransferIoHandler::id() const
{
    return m_id;
}

void SftpTransferIoHandler::setCompletedHook(std::function<void()> completed)
{
    m_hooks.completed = std::move(completed);
}

bool SftpTransferIoHandler::start(SshIoLoop *loop, QString *error)
{
    if (m_started) {
        return true;
    }
    if (loop == nullptr || m_fs == nullptr || !m_fs->isOpen()) {
        if (error != nullptr) {
            *error = QStringLiteral("SftpTransferIoHandler: missing loop or remote FS");
        }
        return false;
    }
    m_loop = loop;
    m_started = true;
    m_cancelled = false;
    m_loop->wake();
    return true;
}

void SftpTransferIoHandler::cancel()
{
    if (m_cancelled) {
        return;
    }
    m_cancelled = true;
    if (m_fs != nullptr) {
        if (m_begun) {
            m_fs->requestCancel();
        }
        // Handler is leaving the loop — stop async work immediately.
        if (m_fs->hasAsyncTransfer()) {
            m_fs->abortAsyncTransfer();
        }
    }
    if (!m_finished && m_started) {
        m_finished = true;
        if (m_hooks.completed) {
            m_hooks.completed();
        }
    } else {
        m_finished = true;
    }
}

void SftpTransferIoHandler::finishOk()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    if (m_hooks.finished) {
        m_hooks.finished(m_request.finishedMessage);
    }
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}

void SftpTransferIoHandler::finishFail(const QString &error)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    if (m_hooks.failed) {
        m_hooks.failed(error);
    }
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}

void SftpTransferIoHandler::onIdle()
{
    if (!m_started || m_finished) {
        return;
    }
    if (m_cancelled && !m_begun) {
        return;
    }

    if (!m_begun) {
        QString error;
        bool ok = false;
        switch (m_request.kind) {
        case Kind::UploadFiles:
            ok = m_fs->beginAsyncUploadFiles(m_request.localPaths, m_request.remoteDir, &error);
            break;
        case Kind::UploadFileTo:
            ok = m_fs->beginAsyncUploadFileTo(m_request.localPath, m_request.remotePath, &error);
            break;
        case Kind::DownloadPaths:
            ok = m_fs->beginAsyncDownloadPaths(
                m_request.remotePaths, m_request.localDir, &error, m_request.followSymlinks);
            break;
        case Kind::ResumeInterrupted:
            ok = m_fs->beginAsyncResumeInterrupted(&error);
            break;
        }
        if (!ok) {
            finishFail(error);
            return;
        }
        m_begun = true;
        if (m_loop != nullptr) {
            m_loop->wake();
        }
        return;
    }

    QString error;
    const FsRemote::TickResult result = m_fs->tickAsync(&error);
    switch (result) {
    case FsRemote::TickResult::Idle:
        finishOk();
        break;
    case FsRemote::TickResult::Running:
        if (m_loop != nullptr) {
            m_loop->wake();
        }
        break;
    case FsRemote::TickResult::Done:
        finishOk();
        break;
    case FsRemote::TickResult::Failed:
        finishFail(error);
        break;
    }
}
