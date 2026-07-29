#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>

class QSocketNotifier;
class QTermWidget;
class QTimer;

/**
 * Platform I/O between SSH channel bytes and QTermWidget.
 *
 * Unix: write through the teletype PTY slave fd (backpressure via QSocketNotifier).
 * Windows: feed Emulation via QTermWidget::writeToEmulator (patched API, no local PTY).
 */
class TerminalIoBridge final : public QObject
{
    Q_OBJECT

public:
    explicit TerminalIoBridge(QObject *parent = nullptr);
    ~TerminalIoBridge() override;

    void start(QTermWidget *term);
    void teardown();

    /** Queue / deliver remote VT bytes to the emulator. */
    void feed(const QByteArray &data);

    /** Sync local emulator size (cols x rows). SSH resize is done by the caller. */
    void syncSize(int cols, int rows);

    bool isActive() const;

private slots:
    void flushPending();

private:
    void setupNotifier();

    QPointer<QTermWidget> m_term;
    QByteArray m_pending;
    QSocketNotifier *m_writeNotifier = nullptr;
    QTimer *m_flushTimer = nullptr;
    bool m_active = false;
};
