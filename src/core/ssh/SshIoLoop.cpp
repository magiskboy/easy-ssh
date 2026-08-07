/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "core/ssh/SshIoLoop.h"

#include <cerrno>
#include <cstring>

#include <QCoreApplication>
#include <QEventLoop>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#ifdef Q_OS_LINUX
#include <sys/eventfd.h>
#endif
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace
{

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

#ifdef Q_OS_WIN
bool setSocketNonBlocking(socket_t fd)
{
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
}

void closeSocket(socket_t fd)
{
    if (fd != SSH_INVALID_SOCKET) {
        closesocket(fd);
    }
}
#else
bool setSocketNonBlocking(socket_t fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void closeSocket(socket_t fd)
{
    if (fd != SSH_INVALID_SOCKET) {
        ::close(static_cast<int>(fd));
    }
}
#endif

} // namespace

SshIoLoop::SshIoLoop(QObject *parent) : QObject(parent)
{
    m_event = ssh_event_new();
    QString wakeError;
    if (m_event == nullptr) {
        return;
    }
    if (!openWakeFd(&wakeError) || !registerWakeFd(&wakeError)) {
        closeWakeFd();
        destroyEvent();
    }
}

SshIoLoop::~SshIoLoop()
{
    stop();
    detachSession();
    removeAllUserFds();
    unregisterWakeFd();
    closeWakeFd();
    destroyEvent();
}

bool SshIoLoop::attachSession(ssh_session session, QString *error)
{
    if (m_event == nullptr) {
        setError(error, QStringLiteral("ssh_event not initialized"));
        return false;
    }
    if (session == nullptr) {
        setError(error, QStringLiteral("null ssh_session"));
        return false;
    }
    if (m_session != nullptr) {
        setError(error, QStringLiteral("session already attached"));
        return false;
    }

    ssh_set_blocking(session, 0);
    if (ssh_event_add_session(m_event, session) != SSH_OK) {
        setError(error, QStringLiteral("ssh_event_add_session failed"));
        return false;
    }
    m_session = session;
    return true;
}

void SshIoLoop::detachSession()
{
    cancelAllHandlers();
    unregisterAllChannels();

    if (m_event != nullptr && m_session != nullptr) {
        ssh_event_remove_session(m_event, m_session);
    }
    m_session = nullptr;
}

void SshIoLoop::run()
{
    if (m_event == nullptr) {
        emit fault(QStringLiteral("ssh_event not initialized"));
        return;
    }
    if (m_running.exchange(true)) {
        emit fault(QStringLiteral("SshIoLoop::run already active"));
        return;
    }

    m_stopped.store(false);

    // Finite timeout so QTimer (tunnels/agent/keepalive) and QueuedConnection slots
    // can run via processEvents while SSH is idle. wake() still returns early.
    constexpr int kIdlePollMs = 20;

    while (!m_stopped.load()) {
        const int rc = ssh_event_dopoll(m_event, kIdlePollMs);
        if (m_stopped.load()) {
            break;
        }

        if (rc == SSH_ERROR) {
#ifdef Q_OS_WIN
            const int err = WSAGetLastError();
            if (err == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            emit fault(QStringLiteral("ssh_event_dopoll failed"));
            break;
        }

        processQueuedEvents();
        if (m_stopped.load()) {
            break;
        }
        invokeOnIdle();

        if (m_session != nullptr && !ssh_is_connected(m_session)) {
            emit sessionEof();
            break;
        }
    }

    m_running.store(false);
}

bool SshIoLoop::pollOnce(int timeoutMs)
{
    if (m_event == nullptr) {
        return false;
    }

    const int rc = ssh_event_dopoll(m_event, timeoutMs);
    if (rc == SSH_ERROR) {
#ifdef Q_OS_WIN
        if (WSAGetLastError() == WSAEINTR) {
            invokeOnIdle();
            return true;
        }
#else
        if (errno == EINTR) {
            invokeOnIdle();
            return true;
        }
#endif
        return false;
    }
    invokeOnIdle();
    return true;
}

void SshIoLoop::invokeOnIdle()
{
    std::vector<SshIoHandler *> idleTargets;
    idleTargets.reserve(m_handlers.size());
    for (const auto &handler : m_handlers) {
        if (handler) {
            idleTargets.push_back(handler.get());
        }
    }
    for (SshIoHandler *handler : idleTargets) {
        handler->onIdle();
    }
}

void SshIoLoop::processQueuedEvents()
{
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

void SshIoLoop::stop()
{
    m_stopped.store(true);
    wake();
}

void SshIoLoop::wake()
{
    writeWake();
}

bool SshIoLoop::addHandler(std::unique_ptr<SshIoHandler> handler, QString *error)
{
    if (!handler) {
        setError(error, QStringLiteral("null handler"));
        return false;
    }
    const QString id = handler->id();
    if (id.isEmpty()) {
        setError(error, QStringLiteral("handler id is empty"));
        return false;
    }
    for (const auto &existing : m_handlers) {
        if (existing && existing->id() == id) {
            setError(error, QStringLiteral("handler id already registered: %1").arg(id));
            return false;
        }
    }

    QString startError;
    if (!handler->start(this, &startError)) {
        setError(error, startError.isEmpty() ? QStringLiteral("handler start failed") : startError);
        return false;
    }
    m_handlers.push_back(std::move(handler));
    return true;
}

void SshIoLoop::removeHandler(const QString &handlerId)
{
    for (auto it = m_handlers.begin(); it != m_handlers.end(); ++it) {
        if (*it && (*it)->id() == handlerId) {
            (*it)->cancel();
            m_handlers.erase(it);
            return;
        }
    }
}

SshIoHandler *SshIoLoop::handler(const QString &handlerId) const
{
    for (const auto &existing : m_handlers) {
        if (existing && existing->id() == handlerId) {
            return existing.get();
        }
    }
    return nullptr;
}

bool SshIoLoop::addFd(socket_t fd, short events, FdCallback cb, QString *error)
{
    if (m_event == nullptr) {
        setError(error, QStringLiteral("ssh_event not initialized"));
        return false;
    }
    if (fd == SSH_INVALID_SOCKET) {
        setError(error, QStringLiteral("invalid fd"));
        return false;
    }
    if (!cb) {
        setError(error, QStringLiteral("null fd callback"));
        return false;
    }
    if (m_fds.contains(fd) || fd == m_wakeRead) {
        setError(error, QStringLiteral("fd already registered"));
        return false;
    }

    if (ssh_event_add_fd(m_event, fd, events, &SshIoLoop::userFdCallback, this) != SSH_OK) {
        setError(error, QStringLiteral("ssh_event_add_fd failed"));
        return false;
    }
    m_fds.insert(fd, FdRegistration{std::move(cb)});
    return true;
}

void SshIoLoop::removeFd(socket_t fd)
{
    if (m_event == nullptr || !m_fds.contains(fd)) {
        return;
    }
    ssh_event_remove_fd(m_event, fd);
    m_fds.remove(fd);
}

bool SshIoLoop::registerChannel(ssh_channel channel, SshChannelCallbacks *sink, QString *error)
{
    if (channel == nullptr || sink == nullptr) {
        setError(error, QStringLiteral("null channel or sink"));
        return false;
    }
    if (m_channels.contains(channel)) {
        setError(error, QStringLiteral("channel already registered"));
        return false;
    }

    auto *reg = new ChannelRegistration;
    reg->channel = channel;
    reg->sink = sink;
    ssh_callbacks_init(&reg->cbs);
    reg->cbs.userdata = reg;
    reg->cbs.channel_data_function = &SshIoLoop::channelDataTrampoline;
    reg->cbs.channel_eof_function = &SshIoLoop::channelEofTrampoline;
    reg->cbs.channel_close_function = &SshIoLoop::channelCloseTrampoline;

    if (ssh_set_channel_callbacks(channel, &reg->cbs) != SSH_OK) {
        delete reg;
        setError(error, QStringLiteral("ssh_set_channel_callbacks failed"));
        return false;
    }
    m_channels.insert(channel, reg);
    return true;
}

void SshIoLoop::unregisterChannel(ssh_channel channel)
{
    ChannelRegistration *reg = m_channels.take(channel);
    if (reg == nullptr) {
        return;
    }
    ssh_remove_channel_callbacks(channel, &reg->cbs);
    delete reg;
}

bool SshIoLoop::openWakeFd(QString *error)
{
#ifdef Q_OS_WIN
    socket_t listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == SSH_INVALID_SOCKET) {
        setError(error, QStringLiteral("wake listener socket failed"));
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (::bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
        ::listen(listener, 1) != 0) {
        closeSocket(listener);
        setError(error, QStringLiteral("wake bind/listen failed"));
        return false;
    }

    int addrLen = sizeof(addr);
    if (::getsockname(listener, reinterpret_cast<sockaddr *>(&addr), &addrLen) != 0) {
        closeSocket(listener);
        setError(error, QStringLiteral("wake getsockname failed"));
        return false;
    }

    m_wakeWrite = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_wakeWrite == SSH_INVALID_SOCKET) {
        closeSocket(listener);
        setError(error, QStringLiteral("wake write socket failed"));
        return false;
    }

    if (::connect(m_wakeWrite, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        closeSocket(m_wakeWrite);
        m_wakeWrite = SSH_INVALID_SOCKET;
        closeSocket(listener);
        setError(error, QStringLiteral("wake connect failed"));
        return false;
    }

    m_wakeRead = ::accept(listener, nullptr, nullptr);
    closeSocket(listener);
    if (m_wakeRead == SSH_INVALID_SOCKET) {
        closeSocket(m_wakeWrite);
        m_wakeWrite = SSH_INVALID_SOCKET;
        setError(error, QStringLiteral("wake accept failed"));
        return false;
    }

    if (!setSocketNonBlocking(m_wakeRead) || !setSocketNonBlocking(m_wakeWrite)) {
        closeWakeFd();
        setError(error, QStringLiteral("wake set nonblocking failed"));
        return false;
    }
    return true;
#elif defined(Q_OS_LINUX)
    const int efd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        setError(error,
                 QStringLiteral("eventfd failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }
    m_wakeRead = static_cast<socket_t>(efd);
    m_wakeWrite = m_wakeRead; // same fd for read/write
    return true;
#else
    int fds[2] = {-1, -1};
#ifdef O_CLOEXEC
    if (::pipe2(fds, O_NONBLOCK | O_CLOEXEC) != 0) {
#else
    if (::pipe(fds) != 0) {
#endif
        setError(error,
                 QStringLiteral("pipe failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }
#ifndef O_CLOEXEC
    if (!setSocketNonBlocking(static_cast<socket_t>(fds[0])) ||
        !setSocketNonBlocking(static_cast<socket_t>(fds[1]))) {
        ::close(fds[0]);
        ::close(fds[1]);
        setError(error, QStringLiteral("pipe set nonblocking failed"));
        return false;
    }
#endif
    m_wakeRead = static_cast<socket_t>(fds[0]);
    m_wakeWrite = static_cast<socket_t>(fds[1]);
    return true;
#endif
}

void SshIoLoop::closeWakeFd()
{
#ifdef Q_OS_LINUX
    if (m_wakeRead != SSH_INVALID_SOCKET) {
        closeSocket(m_wakeRead);
    }
    m_wakeRead = SSH_INVALID_SOCKET;
    m_wakeWrite = SSH_INVALID_SOCKET;
#else
    if (m_wakeRead != SSH_INVALID_SOCKET) {
        closeSocket(m_wakeRead);
        m_wakeRead = SSH_INVALID_SOCKET;
    }
    if (m_wakeWrite != SSH_INVALID_SOCKET && m_wakeWrite != m_wakeRead) {
        closeSocket(m_wakeWrite);
        m_wakeWrite = SSH_INVALID_SOCKET;
    } else {
        m_wakeWrite = SSH_INVALID_SOCKET;
    }
#endif
}

bool SshIoLoop::registerWakeFd(QString *error)
{
    if (m_event == nullptr || m_wakeRead == SSH_INVALID_SOCKET) {
        setError(error, QStringLiteral("wake fd not ready"));
        return false;
    }
    if (ssh_event_add_fd(m_event, m_wakeRead, POLLIN, &SshIoLoop::wakeFdCallback, this) != SSH_OK) {
        setError(error, QStringLiteral("ssh_event_add_fd(wake) failed"));
        return false;
    }
    m_wakeRegistered = true;
    return true;
}

void SshIoLoop::unregisterWakeFd()
{
    if (m_event != nullptr && m_wakeRegistered && m_wakeRead != SSH_INVALID_SOCKET) {
        ssh_event_remove_fd(m_event, m_wakeRead);
    }
    m_wakeRegistered = false;
}

void SshIoLoop::drainWake()
{
    if (m_wakeRead == SSH_INVALID_SOCKET) {
        return;
    }
#ifdef Q_OS_LINUX
    uint64_t value = 0;
    while (::read(static_cast<int>(m_wakeRead), &value, sizeof(value)) > 0) {
    }
#elif defined(Q_OS_WIN)
    char buf[64];
    for (;;) {
        const int n = ::recv(m_wakeRead, buf, sizeof(buf), 0);
        if (n <= 0) {
            break;
        }
    }
#else
    char buf[64];
    while (::read(static_cast<int>(m_wakeRead), buf, sizeof(buf)) > 0) {
    }
#endif
}

void SshIoLoop::writeWake()
{
    if (m_wakeWrite == SSH_INVALID_SOCKET) {
        return;
    }
#ifdef Q_OS_LINUX
    const uint64_t one = 1;
    const ssize_t n = ::write(static_cast<int>(m_wakeWrite), &one, sizeof(one));
    (void)n;
#elif defined(Q_OS_WIN)
    const char byte = 1;
    (void)::send(m_wakeWrite, &byte, 1, 0);
#else
    const char byte = 1;
    (void)::write(static_cast<int>(m_wakeWrite), &byte, 1);
#endif
}

void SshIoLoop::cancelAllHandlers()
{
    for (auto &handler : m_handlers) {
        if (handler) {
            handler->cancel();
        }
    }
    m_handlers.clear();
}

void SshIoLoop::unregisterAllChannels()
{
    const auto keys = m_channels.keys();
    for (ssh_channel channel : keys) {
        unregisterChannel(channel);
    }
}

void SshIoLoop::removeAllUserFds()
{
    const auto keys = m_fds.keys();
    for (socket_t fd : keys) {
        removeFd(fd);
    }
}

void SshIoLoop::destroyEvent()
{
    if (m_event != nullptr) {
        ssh_event_free(m_event);
        m_event = nullptr;
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int SshIoLoop::wakeFdCallback(socket_t fd, int revents, void *userdata)
{
    Q_UNUSED(fd);
    auto *self = static_cast<SshIoLoop *>(userdata);
    if (self == nullptr) {
        return -1;
    }
    if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return -1;
    }
    if (revents & POLLIN) {
        self->drainWake();
    }
    return 0;
}

int SshIoLoop::userFdCallback(socket_t fd, int revents, void *userdata)
{
    auto *self = static_cast<SshIoLoop *>(userdata);
    if (self == nullptr) {
        return -1;
    }
    const auto it = self->m_fds.constFind(fd);
    if (it == self->m_fds.cend() || !it->callback) {
        return -1;
    }
    return it->callback(fd, revents);
}

int SshIoLoop::channelDataTrampoline(ssh_session session,
                                     ssh_channel channel,
                                     void *data,
                                     uint32_t len,
                                     int isStderr,
                                     void *userdata)
{
    auto *reg = static_cast<ChannelRegistration *>(userdata);
    if (reg == nullptr || reg->sink == nullptr) {
        return static_cast<int>(len);
    }
    return reg->sink->onData(session, channel, data, len, isStderr);
}

void SshIoLoop::channelEofTrampoline(ssh_session session, ssh_channel channel, void *userdata)
{
    auto *reg = static_cast<ChannelRegistration *>(userdata);
    if (reg == nullptr || reg->sink == nullptr) {
        return;
    }
    reg->sink->onEof(session, channel);
}

void SshIoLoop::channelCloseTrampoline(ssh_session session, ssh_channel channel, void *userdata)
{
    auto *reg = static_cast<ChannelRegistration *>(userdata);
    if (reg == nullptr || reg->sink == nullptr) {
        return;
    }
    reg->sink->onClose(session, channel);
}
