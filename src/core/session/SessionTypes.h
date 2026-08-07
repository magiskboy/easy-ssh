/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QUuid>

enum class SessionState
{
    Connecting,
    Connected,
    Disconnected,
    Failed,
};

enum class ChannelState
{
    Opening,
    Open,
    Closed,
    Failed,
};

enum class TunnelRunStatus
{
    Off,
    Starting,
    Running,
    Error,
};

enum class FsBackend
{
    None = 0,
    Sftp = 1,
    Scp = 2,
};

struct TerminalChannelState
{
    QUuid id;
    QString title;
    int serial = 1;
    ChannelState state = ChannelState::Opening;
    int cols = 80;
    int rows = 24;
    QDateTime createdAt;
    /// Hidden from SessionPage panes / Go to Terminal; owned by a secondary UI (e.g. logs dialog).
    bool auxiliary = false;
};

struct FileChannelState
{
    bool available = false;
    FsBackend backend = FsBackend::None;
    ChannelState state = ChannelState::Closed;
    QString cwd;
    QString unavailableReason;
};

struct TunnelChannelState
{
    QUuid tunnelId;
    TunnelRunStatus status = TunnelRunStatus::Off;
    QString detail;
};

Q_DECLARE_METATYPE(SessionState)
Q_DECLARE_METATYPE(ChannelState)
Q_DECLARE_METATYPE(TunnelRunStatus)
Q_DECLARE_METATYPE(FsBackend)
Q_DECLARE_METATYPE(TerminalChannelState)
Q_DECLARE_METATYPE(FileChannelState)
Q_DECLARE_METATYPE(TunnelChannelState)
