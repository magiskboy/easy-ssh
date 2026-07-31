/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/session/SessionTypes.h"
#include "core/ssh/SshWorker.h"
#include "gui/ErrorNotifier.h"

#include <QHash>
#include <QUuid>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTermWidget;
class QTimer;
class Session;
class TerminalIoBridge;

class SessionPage final : public QWidget
{
    Q_OBJECT

public:
    explicit SessionPage(Session *session, QWidget *parent = nullptr);
    ~SessionPage() override;

    Session *session() const { return m_session; }

    void applySettings();
    void copySelection();
    void pasteClipboard();
    void toggleSearch();
    void clearScreen();
    void saveLog();
    void saveScreenshot();

signals:
    void statusMessage(const QString &message, ErrorNotifier::Level level);
    void editRequested();
    void closeRequested();
    void deleteRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSessionStateChanged(SessionState state);
    void onShellsChanged();
    void onActiveShellChanged(const QUuid &shellId);
    void onShellData(const QUuid &shellId, const QByteArray &data);
    void onHostKeyPrompt(SshWorker::HostKeyPrompt reason,
                         const QString &fingerprint,
                         const QString &contextLabel);
    void onSendData(const char *data, int length);
    void syncPtySize();
    void disconnectOrReconnect();

private:
    struct Pane
    {
        QTermWidget *term = nullptr;
        TerminalIoBridge *bridge = nullptr;
        bool teletypeStarted = false;
        int lastCols = 0;
        int lastRows = 0;
    };

    void ensurePane(const QUuid &shellId);
    void removePane(const QUuid &shellId);
    void applySettingsToTerm(QTermWidget *term);
    Pane *activePane();
    QTermWidget *activeTerm();
    void showOverlay(const QString &message, bool showReconnect);
    void schedulePtySizeSync();
    QSize readTerminalSize(QTermWidget *term) const;

    Session *m_session = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_overlay = nullptr;
    QLabel *m_overlayLabel = nullptr;
    QPushButton *m_reconnectButton = nullptr;
    QHash<QUuid, Pane> m_panes;
    QHash<QUuid, int> m_stackIndex;
    QTimer *m_resizeDebounce = nullptr;
};
