/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/ssh/SshIoHandler.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>

/**
 * High-level I/O loop for ONE ssh_session on ONE worker thread.
 *
 * Threading rules:
 * - Call run() and all libssh-touching methods (attachSession, addHandler, addFd,
 *   registerChannel, …) only from the loop/worker thread (or while the loop is
 *   not running).
 * - stop() and wake() are thread-safe and may be called from any thread.
 * - Channel / fd callbacks run during ssh_event_dopoll; enqueue only, do heavy
 *   work in SshIoHandler::onIdle() after dopoll returns.
 *
 * Does not own or free the ssh_session — SshSession / SshWorker does.
 */
class SshIoLoop final : public QObject
{
    Q_OBJECT

public:
    using FdCallback = std::function<int(socket_t fd, int revents)>;

    explicit SshIoLoop(QObject *parent = nullptr);
    ~SshIoLoop() override;

    SshIoLoop(const SshIoLoop &) = delete;
    SshIoLoop &operator=(const SshIoLoop &) = delete;

    bool attachSession(ssh_session session, QString *error = nullptr);
    void detachSession();
    ssh_session session() const { return m_session; }
    bool isAttached() const { return m_session != nullptr; }

    /// Blocks until stop(). Must run on the loop thread.
    /// Uses a short dopoll timeout so Qt timers / processEvents can run while idle.
    void run();
    /// One dopoll + onIdle. For nested pump while run() is blocked in processEvents
    /// (e.g. leftover sync SCP). Does not call processEvents (caller may). Same thread only.
    bool pollOnce(int timeoutMs);
    /// Thread-safe: set stop flag and wake dopoll.
    void stop();
    /// Thread-safe: wake dopoll so queued work can run.
    void wake();

    bool addHandler(std::unique_ptr<SshIoHandler> handler, QString *error = nullptr);
    void removeHandler(const QString &handlerId);
    SshIoHandler *handler(const QString &handlerId) const;

    bool addFd(socket_t fd, short events, FdCallback cb, QString *error = nullptr);
    void removeFd(socket_t fd);

    /// Install channel data/eof/close trampolines. @p sink must outlive unregister.
    bool registerChannel(ssh_channel channel, SshChannelCallbacks *sink, QString *error = nullptr);
    void unregisterChannel(ssh_channel channel);

signals:
    void fault(const QString &message);
    void sessionEof();

private:
    struct ChannelRegistration
    {
        ssh_channel channel = nullptr;
        SshChannelCallbacks *sink = nullptr;
        struct ssh_channel_callbacks_struct cbs{};
    };

    struct FdRegistration
    {
        FdCallback callback;
    };

    bool openWakeFd(QString *error);
    void closeWakeFd();
    bool registerWakeFd(QString *error);
    void unregisterWakeFd();
    void drainWake();
    void writeWake();

    void cancelAllHandlers();
    void unregisterAllChannels();
    void removeAllUserFds();
    void destroyEvent();
    void invokeOnIdle();
    void processQueuedEvents();

    static int wakeFdCallback(socket_t fd, int revents, void *userdata);
    static int userFdCallback(socket_t fd, int revents, void *userdata);
    static int channelDataTrampoline(ssh_session session,
                                     ssh_channel channel,
                                     void *data,
                                     uint32_t len,
                                     int isStderr,
                                     void *userdata);
    static void channelEofTrampoline(ssh_session session, ssh_channel channel, void *userdata);
    static void channelCloseTrampoline(ssh_session session, ssh_channel channel, void *userdata);
    static void channelExitStatusTrampoline(ssh_session session,
                                            ssh_channel channel,
                                            int exitStatus,
                                            void *userdata);

    ssh_event m_event = nullptr;
    ssh_session m_session = nullptr;
    std::atomic_bool m_stopped{false};
    std::atomic_bool m_running{false};

    socket_t m_wakeRead = SSH_INVALID_SOCKET;
    socket_t m_wakeWrite = SSH_INVALID_SOCKET;
    bool m_wakeRegistered = false;

    std::vector<std::unique_ptr<SshIoHandler>> m_handlers;
    QHash<socket_t, FdRegistration> m_fds;
    QHash<ssh_channel, ChannelRegistration *> m_channels;
};
