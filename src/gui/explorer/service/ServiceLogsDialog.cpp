// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "ServiceLogsDialog.h"

#include "core/explorer/service/ServiceParser.h"
#include "core/session/Session.h"
#include "core/settings/AppSettings.h"
#include "gui/dialogs/ModelessDialog.h"
#include "gui/terminal/TerminalIoBridge.h"
#include "gui/widgets/UiMetrics.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include <qtermwidget.h>

namespace
{
constexpr int kMinCols = 2;
constexpr int kMinRows = 2;
constexpr int kDefaultCols = 80;
constexpr int kDefaultRows = 24;
constexpr int kFollowLines = 100;
} // namespace

ServiceLogsDialog::ServiceLogsDialog(Session *session, const ServiceInfo &service, QWidget *parent)
    : QDialog(parent), m_session(session), m_service(service)
{
    configureModelessDialog(this, UiMetrics::dialogMinWidth);
    setWindowTitle(tr("Logs: %1").arg(service.unit));
    setMinimumHeight(420);
    resize(720, 480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing,
                             UiMetrics::sectionSpacing);
    root->setSpacing(UiMetrics::relatedSpacing);

    m_statusLabel = new QLabel(tr("Opening log stream…"), this);
    root->addWidget(m_statusLabel);

    m_term = new QTermWidget(0, this);
    m_term->setTerminalSizeHint(false);
    m_term->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_term->installEventFilter(this);
    m_bridge = new TerminalIoBridge(m_term);
    applySettingsToTerm();
    root->addWidget(m_term, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);

    m_resizeDebounce = new QTimer(this);
    m_resizeDebounce->setSingleShot(true);
    m_resizeDebounce->setInterval(50);
    connect(m_resizeDebounce, &QTimer::timeout, this, &ServiceLogsDialog::syncPtySize);

    connect(this, &QDialog::finished, this, &ServiceLogsDialog::closeOwnedTerminal);

    if (!m_session || m_session->state() != SessionState::Connected) {
        m_statusLabel->setText(tr("No connected session available for logs."));
        m_term->setEnabled(false);
        return;
    }

    const QString command = ServiceParser::followLogsCommand(m_service, kFollowLines);
    if (command.isEmpty()) {
        m_statusLabel->setText(tr("Live logs are not supported for this service manager."));
        m_term->setEnabled(false);
        return;
    }

    connect(m_session, &Session::terminalsChanged, this, &ServiceLogsDialog::onTerminalsChanged);
    connect(m_session, &Session::shellData, this, &ServiceLogsDialog::onTerminalData);
    connect(m_session, &Session::stateChanged, this, &ServiceLogsDialog::onSessionStateChanged);
    connect(m_term, SIGNAL(sendData(const char *, int)), this, SLOT(onSendData(const char *, int)));

    // Open with a sane default; real size is synced after the dialog is shown/laid out.
    m_terminalId = m_session->newTerminal(kDefaultCols, kDefaultRows, {}, true);
    if (m_terminalId.isNull()) {
        m_statusLabel->setText(tr("Could not open a terminal channel for logs."));
        m_term->setEnabled(false);
        return;
    }

    m_session->renameTerminal(m_terminalId, tr("Logs: %1").arg(m_service.unit));
    m_term->startTerminalTeletype();
    m_teletypeStarted = true;
    m_bridge->start(m_term);
}

ServiceLogsDialog::~ServiceLogsDialog()
{
    closeOwnedTerminal();
}

bool ServiceLogsDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_term && event->type() == QEvent::Resize) {
        schedulePtySizeSync();
    }
    return QDialog::eventFilter(watched, event);
}

void ServiceLogsDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    schedulePtySizeSync();
}

void ServiceLogsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // Match TerminalSessionWidget: sync after layout settles.
    schedulePtySizeSync();
    QTimer::singleShot(0, this, &ServiceLogsDialog::syncPtySize);
    QTimer::singleShot(150, this, &ServiceLogsDialog::syncPtySize);
}

void ServiceLogsDialog::onTerminalsChanged()
{
    tryInjectCommand();

    if (m_terminalId.isNull() || !m_session || m_closingShell) {
        return;
    }

    bool found = false;
    for (const TerminalChannelState &shell : m_session->terminals()) {
        if (shell.id == m_terminalId) {
            found = true;
            if (shell.state == ChannelState::Failed) {
                m_statusLabel->setText(tr("Log terminal failed."));
                m_statusLabel->show();
            }
            break;
        }
    }
    if (!found) {
        m_terminalId = {};
        m_statusLabel->setText(tr("Log terminal closed."));
        m_statusLabel->show();
        m_term->setEnabled(false);
    }
}

void ServiceLogsDialog::onTerminalData(const QUuid &terminalId, const QByteArray &data)
{
    if (terminalId != m_terminalId || data.isEmpty() || !m_bridge) {
        return;
    }
    if (!m_teletypeStarted && m_term) {
        m_term->startTerminalTeletype();
        m_teletypeStarted = true;
        m_bridge->start(m_term);
    }
    if (m_statusLabel && m_statusLabel->isVisible()) {
        m_statusLabel->hide();
        schedulePtySizeSync();
    }
    m_bridge->feed(data);
}

void ServiceLogsDialog::onSendData(const char *data, int length)
{
    if (!m_session || m_terminalId.isNull() || data == nullptr || length <= 0) {
        return;
    }
    m_session->writeToTerminal(m_terminalId, QByteArray(data, length));
}

void ServiceLogsDialog::onSessionStateChanged()
{
    if (!m_session || m_session->state() == SessionState::Connected) {
        return;
    }
    m_statusLabel->setText(tr("Session disconnected."));
    m_statusLabel->show();
    m_term->setEnabled(false);
    closeOwnedTerminal();
}

void ServiceLogsDialog::syncPtySize()
{
    if (!m_session || m_terminalId.isNull() || m_session->state() != SessionState::Connected ||
        !m_term || !m_teletypeStarted) {
        return;
    }

    if (!m_term->isVisible() || m_term->width() < 20 || m_term->height() < 20) {
        schedulePtySizeSync();
        return;
    }

    // QTermWidget::resizeEvent manually resizes TerminalDisplay to this->size().
    // When the parent layout changes geometry, that path can lag — force it before
    // reading columns so local grid and remote PTY stay aligned.
    {
        QResizeEvent force(m_term->size(), m_term->size());
        QCoreApplication::sendEvent(m_term, &force);
    }

    const QSize pixels = m_term->size();
    const QSize sz = readTerminalSize();
    if (sz.width() < kMinCols || sz.height() < kMinRows) {
        schedulePtySizeSync();
        return;
    }
    if (pixels == m_lastPixels && sz.width() == m_lastCols && sz.height() == m_lastRows) {
        return;
    }

    m_lastPixels = pixels;
    m_lastCols = sz.width();
    m_lastRows = sz.height();

    // Remote SSH PTY only. Local emulation/PTY are owned by QTermWidget's resize path;
    // ioctl/setEmulationSize here fights that and can freeze the grid size.
    m_session->changePtySize(m_terminalId, sz.width(), sz.height());
}

void ServiceLogsDialog::applySettingsToTerm()
{
    if (!m_term) {
        return;
    }
    const auto &settings = AppSettings::instance();
    m_term->setTerminalFont(settings.terminalFont());
    m_term->setColorScheme(settings.colorScheme());
    m_term->setHistorySize(settings.historySize());
    m_term->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_term->setBlinkingCursor(settings.cursorBlink());
    m_term->setConfirmMultilinePaste(settings.confirmMultilinePaste());

    using Shape = QTermWidget::KeyboardCursorShape;
    Shape shape = Shape::BlockCursor;
    switch (settings.cursorShape()) {
    case 1:
        shape = Shape::UnderlineCursor;
        break;
    case 2:
        shape = Shape::IBeamCursor;
        break;
    default:
        shape = Shape::BlockCursor;
        break;
    }
    m_term->setKeyboardCursorShape(shape);
}

void ServiceLogsDialog::schedulePtySizeSync()
{
    if (m_resizeDebounce) {
        m_resizeDebounce->start();
    }
}

void ServiceLogsDialog::tryInjectCommand()
{
    if (m_commandInjected || m_terminalId.isNull() || !m_session) {
        return;
    }

    bool isOpen = false;
    for (const TerminalChannelState &shell : m_session->terminals()) {
        if (shell.id == m_terminalId && shell.state == ChannelState::Open) {
            isOpen = true;
            break;
        }
    }
    if (!isOpen) {
        return;
    }

    // Sync size before starting journalctl so the first paint uses the dialog size.
    syncPtySize();

    const QString command = ServiceParser::followLogsCommand(m_service, kFollowLines);
    if (command.isEmpty()) {
        m_statusLabel->setText(tr("Live logs are not supported for this service manager."));
        m_statusLabel->show();
        return;
    }

    m_commandInjected = true;
    m_statusLabel->hide();
    schedulePtySizeSync();
    m_session->writeToTerminal(m_terminalId, (command + QLatin1Char('\n')).toUtf8());
}

void ServiceLogsDialog::closeOwnedTerminal()
{
    if (m_closingShell) {
        return;
    }
    m_closingShell = true;

    if (m_bridge) {
        m_bridge->teardown();
    }

    if (m_session && !m_terminalId.isNull()) {
        const QUuid id = m_terminalId;
        m_terminalId = {};
        m_session->closeTerminal(id);
    }
}

QSize ServiceLogsDialog::readTerminalSize() const
{
    if (!m_term) {
        return QSize(kDefaultCols, kDefaultRows);
    }

    // Use widget geometry so we detect dialog drag-resize even when
    // screenColumnsCount still reflects the previous emulation size.
    const QFontMetrics fm(m_term->getTerminalFont());
    const int cw = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
    const int ch = qMax(1, fm.height());
    // Roughly match TerminalDisplay: leave room for a non-transient scrollbar.
    constexpr int kScrollbarAllowance = 18;
    constexpr int kMargin = 2;
    const int c = qMax(kMinCols, (m_term->width() - kScrollbarAllowance - kMargin) / cw);
    const int r = qMax(kMinRows, (m_term->height() - kMargin) / ch);
    return QSize(c, r);
}
