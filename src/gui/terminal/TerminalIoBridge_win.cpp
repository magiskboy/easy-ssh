#include "gui/terminal/TerminalIoBridge.h"

#include <QtGlobal>
#include <qtermwidget.h>

namespace
{
constexpr int kMaxPendingDisplayBytes = 1024 * 1024;
constexpr int kMaxFeedChunk = 64 * 1024;
} // namespace

TerminalIoBridge::TerminalIoBridge(QObject *parent) : QObject(parent) {}

TerminalIoBridge::~TerminalIoBridge()
{
    teardown();
}

void TerminalIoBridge::start(QTermWidget *term)
{
    teardown();
    m_term = term;
    m_active = m_term != nullptr;
}

void TerminalIoBridge::teardown()
{
    m_pending.clear();
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
    // Emulation::setImageSize takes (lines, columns).
    m_term->setEmulationSize(rows, cols);
}

void TerminalIoBridge::setupNotifier() {}

void TerminalIoBridge::flushPending()
{
    if (!isActive() || m_pending.isEmpty()) {
        return;
    }

    while (!m_pending.isEmpty()) {
        const int chunk = qMin(kMaxFeedChunk, m_pending.size());
        m_term->writeToEmulator(m_pending.constData(), chunk);
        m_pending.remove(0, chunk);
    }
}
