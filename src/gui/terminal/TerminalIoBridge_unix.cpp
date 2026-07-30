#include "gui/terminal/TerminalIoBridge.h"

#include <QSocketNotifier>
#include <QTimer>

#include <qtermwidget.h>

#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{
constexpr int kMaxPendingDisplayBytes = 1024 * 1024;
constexpr int kFlushRetryMs = 16;
} // namespace

TerminalIoBridge::TerminalIoBridge(QObject *parent) : QObject(parent)
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(kFlushRetryMs);
    connect(m_flushTimer, &QTimer::timeout, this, &TerminalIoBridge::flushPending);
}

TerminalIoBridge::~TerminalIoBridge()
{
    teardown();
}

void TerminalIoBridge::start(QTermWidget *term)
{
    teardown();
    m_term = term;
    if (!m_term) {
        return;
    }
    m_active = true;
    setupNotifier();
}

void TerminalIoBridge::teardown()
{
    if (m_flushTimer) {
        m_flushTimer->stop();
    }
    m_pending.clear();
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        delete m_writeNotifier;
        m_writeNotifier = nullptr;
    }
    m_term = nullptr;
    m_active = false;
}

bool TerminalIoBridge::isActive() const
{
    return m_active && m_term != nullptr;
}

void TerminalIoBridge::feed(const QByteArray &data)
{
    if (!isActive() || data.isEmpty()) {
        return;
    }

    m_pending.append(data);
    if (m_pending.size() > kMaxPendingDisplayBytes) {
        m_pending = m_pending.right(kMaxPendingDisplayBytes / 2);
    }
    flushPending();
}

void TerminalIoBridge::syncSize(int cols, int rows)
{
    if (!isActive() || cols <= 0 || rows <= 0) {
        return;
    }

    const int fd = m_term->getPtySlaveFd();
    if (fd < 0) {
        return;
    }

    struct winsize ws{};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ::ioctl(fd, TIOCSWINSZ, &ws);
}

void TerminalIoBridge::setupNotifier()
{
    const int fd = m_term ? m_term->getPtySlaveFd() : -1;
    if (fd < 0) {
        return;
    }

    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    m_writeNotifier = new QSocketNotifier(fd, QSocketNotifier::Write, this);
    m_writeNotifier->setEnabled(false);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, &TerminalIoBridge::flushPending);
}

void TerminalIoBridge::flushPending()
{
    if (m_pending.isEmpty() || !isActive()) {
        if (m_writeNotifier) {
            m_writeNotifier->setEnabled(false);
        }
        m_flushTimer->stop();
        return;
    }

    const int fd = m_term->getPtySlaveFd();
    if (fd < 0) {
        return;
    }

    while (!m_pending.isEmpty()) {
        const ssize_t written =
            ::write(fd, m_pending.constData(), static_cast<size_t>(m_pending.size()));

        if (written > 0) {
            m_pending.remove(0, static_cast<int>(written));
            continue;
        }

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            if (m_writeNotifier) {
                m_writeNotifier->setEnabled(true);
            }
            if (!m_flushTimer->isActive()) {
                m_flushTimer->start();
            }
            return;
        }

        m_pending.clear();
        break;
    }

    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
    }
    m_flushTimer->stop();
}
