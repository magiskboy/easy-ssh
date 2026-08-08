/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/service/ServiceInfo.h"

#include <QDialog>
#include <QPointer>
#include <QSize>
#include <QUuid>

class QEvent;
class QLabel;
class QResizeEvent;
class QShowEvent;
class QTermWidget;
class QTimer;
class Session;
class TerminalIoBridge;

/// Modeless dialog that follows a unit journal on an auxiliary Session shell.
class ServiceLogsDialog final : public QDialog
{
    Q_OBJECT

public:
    ServiceLogsDialog(Session *session, const ServiceInfo &service, QWidget *parent = nullptr);
    ~ServiceLogsDialog() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onTerminalsChanged();
    void onTerminalData(const QUuid &terminalId, const QByteArray &data);
    void onSendData(const char *data, int length);
    void onSessionStateChanged();
    void syncPtySize();

private:
    void applySettingsToTerm();
    void schedulePtySizeSync();
    void tryInjectCommand();
    void closeOwnedTerminal();
    QSize readTerminalSize() const;

    QPointer<Session> m_session;
    ServiceInfo m_service;
    QUuid m_terminalId;
    QTermWidget *m_term = nullptr;
    TerminalIoBridge *m_bridge = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_resizeDebounce = nullptr;
    bool m_teletypeStarted = false;
    bool m_commandInjected = false;
    bool m_closingShell = false;
    int m_lastCols = 0;
    int m_lastRows = 0;
    QSize m_lastPixels;
};
