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
class QTermWidget;
class QTimer;
class Session;
class ShellDockHost;
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
    void setLayoutActive(bool active);

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
    void onHostKeyPrompt(const QString &fingerprint,
                         SshWorker::HostKeyPrompt reason,
                         const QString &contextLabel);
    void onSendData(const char *data, int length);
    void syncPtySize();
    void disconnectOrReconnect();
    void onDockShellFocused(const QUuid &shellId);
    void onDropShellRequested(const QUuid &shellId, int dockArea);

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
    void pinShellToLayout(const QUuid &shellId, int dockArea = 0x10, const QUuid &relativeTo = {});
    void pinShellWithSmartLayout(const QUuid &shellId);
    void applySettingsToTerm(QTermWidget *term);
    Pane *activePane();
    QTermWidget *activeTerm();
    void showOverlay(const QString &message, bool showReconnect);
    void schedulePtySizeSync();
    QSize readTerminalSize(QTermWidget *term) const;
    QString shellTitle(const QUuid &shellId) const;

    Session *m_session = nullptr;
    ShellDockHost *m_dockHost = nullptr;
    QWidget *m_overlay = nullptr;
    QLabel *m_overlayLabel = nullptr;
    QPushButton *m_reconnectButton = nullptr;
    QHash<QUuid, Pane> m_panes;
    QTimer *m_resizeDebounce = nullptr;
    /// Shell created in the latest shellsChanged; may use smart layout when activated.
    QUuid m_pendingSmartPinId;
};
