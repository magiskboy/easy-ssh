// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "core/ssh/SftpMetaIoHandler.h"

#include "core/fs/Symlink.h"
#include "core/ssh/SshIoLoop.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>

namespace
{
constexpr int kListBatchSize = 64;

QString trMeta(const char *text)
{
    return QCoreApplication::translate("SshWorker", text);
}

void sortEntries(QVector<RemoteEntry> *entries)
{
    if (entries == nullptr) {
        return;
    }
    std::sort(entries->begin(), entries->end(), [](const RemoteEntry &a, const RemoteEntry &b) {
        const bool aDir = Symlink::isDirectoryLike(a);
        const bool bDir = Symlink::isDirectoryLike(b);
        if (aDir != bDir) {
            return aDir;
        }
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
}
} // namespace

SftpMetaIoHandler::SftpMetaIoHandler(FsRemote *fs, Request request, Hooks hooks)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_fs(fs),
      m_request(std::move(request)), m_hooks(std::move(hooks))
{
}

SftpMetaIoHandler::~SftpMetaIoHandler()
{
    cancel();
}

QString SftpMetaIoHandler::id() const
{
    return m_id;
}

void SftpMetaIoHandler::setCompletedHook(std::function<void()> completed)
{
    m_hooks.completed = std::move(completed);
}

bool SftpMetaIoHandler::start(SshIoLoop *loop, QString *error)
{
    if (m_started) {
        return true;
    }
    if (loop == nullptr || m_fs == nullptr || !m_fs->isOpen()) {
        if (error != nullptr) {
            *error = QStringLiteral("SftpMetaIoHandler: missing loop or remote FS");
        }
        return false;
    }
    m_loop = loop;
    m_started = true;
    m_cancelled = false;
    if (m_loop != nullptr) {
        m_loop->wake();
    }
    return true;
}

void SftpMetaIoHandler::cancel()
{
    if (m_cancelled) {
        return;
    }
    m_cancelled = true;
    if (m_listSession && m_fs != nullptr) {
        m_fs->withBlockingSession([&]() {
            m_listSession->close();
            return true;
        });
        m_listSession.reset();
    }
    // Only notify when the handler was successfully started on a loop.
    // Failed start() + destructor must not drive SshWorker FS queue.
    if (!m_finished && m_started) {
        m_finished = true;
        if (m_hooks.completed) {
            m_hooks.completed();
        }
    } else {
        m_finished = true;
    }
}

void SftpMetaIoHandler::finishOk()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}

void SftpMetaIoHandler::finishFail(const QString &message)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    if (m_hooks.failed) {
        m_hooks.failed(message);
    }
    if (m_hooks.completed) {
        m_hooks.completed();
    }
}

void SftpMetaIoHandler::runShortOp()
{
    QString error;
    bool ok = false;

    switch (m_request.op) {
    case Op::CreateDirectory:
        ok = m_fs->withBlockingSession(
            [&]() { return m_fs->engine()->createDirectory(m_request.path, &error); });
        if (ok && m_hooks.finished) {
            m_hooks.finished(trMeta("Created folder: %1").arg(m_request.path));
        }
        break;
    case Op::CreateSymlink:
        ok = m_fs->withBlockingSession([&]() {
            return m_fs->engine()->createSymlink(m_request.target, m_request.linkPath, &error);
        });
        if (ok && m_hooks.finished) {
            m_hooks.finished(trMeta("Created symlink: %1").arg(m_request.linkPath));
        }
        break;
    case Op::RenamePath:
        ok = m_fs->withBlockingSession(
            [&]() { return m_fs->engine()->renamePath(m_request.from, m_request.to, &error); });
        if (ok && m_hooks.finished) {
            m_hooks.finished(trMeta("Renamed to %1").arg(QFileInfo(m_request.to).fileName()));
        }
        break;
    case Op::RemovePath:
        ok = m_fs->removePath(m_request.path, m_request.recursive, &error);
        if (ok && m_hooks.finished) {
            m_hooks.finished(trMeta("Deleted: %1").arg(m_request.path));
        }
        break;
    case Op::CanonicalizePath: {
        const QString requested = m_request.path.isEmpty() ? QStringLiteral(".") : m_request.path;
        QString canonical;
        ok = m_fs->withBlockingSession(
            [&]() { return m_fs->engine()->canonicalizePath(requested, canonical, &error); });
        if (ok && m_hooks.canonicalized) {
            m_hooks.canonicalized(requested, canonical);
        }
        break;
    }
    case Op::ResolveEntry: {
        bool isDir = false;
        ok = m_fs->resolveEntry(m_request.path, &isDir, &error);
        if (!ok) {
            if (m_hooks.resolved) {
                m_hooks.resolved(m_request.path, false, false, error);
            }
            finishOk();
            return;
        }
        if (isDir) {
            m_resolveIsDir = true;
            m_resolveNeedsList = true;
            m_request.op = Op::ListDirectory;
            m_listEntries.clear();
            return; // continue as list in subsequent ticks
        }
        if (m_hooks.resolved) {
            m_hooks.resolved(m_request.path, false, true, {});
        }
        finishOk();
        return;
    }
    case Op::ListDirectory:
        return;
    }

    if (!ok) {
        finishFail(error);
        return;
    }
    finishOk();
}

void SftpMetaIoHandler::tickList()
{
    SftpEngine *sftp = m_fs->sftpEngine();
    if (sftp == nullptr) {
        // SCP / non-SFTP: fall back to one-shot sync list.
        QVector<RemoteEntry> entries;
        QString error;
        if (!m_fs->listDirectoryEntries(m_request.path, &entries, &error)) {
            if (m_resolveNeedsList) {
                if (m_hooks.resolved) {
                    m_hooks.resolved(m_request.path, true, false, error);
                }
                finishOk();
                return;
            }
            finishFail(error);
            return;
        }
        if (m_resolveNeedsList) {
            if (m_hooks.resolved) {
                m_hooks.resolved(m_request.path, true, true, {});
            }
            finishOk();
            return;
        }
        if (m_hooks.listed) {
            m_hooks.listed(m_request.path, entries);
        }
        finishOk();
        return;
    }

    if (!m_listOpened) {
        m_listSession = std::make_unique<SftpEngine::DirListSession>();
        QString error;
        const bool opened = m_fs->withBlockingSession(
            [&]() { return m_listSession->open(sftp, m_request.path, &error); });
        if (!opened) {
            m_listSession.reset();
            if (m_resolveNeedsList) {
                if (m_hooks.resolved) {
                    m_hooks.resolved(m_request.path, true, false, error);
                }
                finishOk();
                return;
            }
            finishFail(error);
            return;
        }
        m_listOpened = true;
    }

    bool eof = false;
    QString error;
    const bool ok = m_fs->withBlockingSession(
        [&]() { return m_listSession->readBatch(kListBatchSize, &m_listEntries, &eof, &error); });
    if (!ok) {
        m_fs->withBlockingSession([&]() {
            m_listSession->close();
            return true;
        });
        m_listSession.reset();
        if (m_resolveNeedsList) {
            if (m_hooks.resolved) {
                m_hooks.resolved(m_request.path, true, false, error);
            }
            finishOk();
            return;
        }
        finishFail(error);
        return;
    }

    if (!eof) {
        if (m_loop != nullptr) {
            m_loop->wake();
        }
        return;
    }

    m_fs->withBlockingSession([&]() {
        m_listSession->close();
        return true;
    });
    m_listSession.reset();
    sortEntries(&m_listEntries);

    if (m_resolveNeedsList) {
        if (m_hooks.resolved) {
            m_hooks.resolved(m_request.path, true, true, {});
        }
        finishOk();
        return;
    }

    if (m_hooks.listed) {
        m_hooks.listed(m_request.path, m_listEntries);
    }
    finishOk();
}

void SftpMetaIoHandler::onIdle()
{
    if (!m_started || m_cancelled || m_finished) {
        return;
    }

    if (m_request.op == Op::ListDirectory) {
        tickList();
        return;
    }

    runShortOp();
    if (!m_finished && !m_cancelled && m_request.op == Op::ListDirectory) {
        tickList();
    }
}
