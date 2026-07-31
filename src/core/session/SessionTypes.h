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

struct ShellChannelState
{
    QUuid id;
    QString title;
    int serial = 1;
    ChannelState state = ChannelState::Opening;
    int cols = 80;
    int rows = 24;
    QDateTime createdAt;
};

struct FileChannelState
{
    bool available = false;
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
Q_DECLARE_METATYPE(ShellChannelState)
Q_DECLARE_METATYPE(FileChannelState)
Q_DECLARE_METATYPE(TunnelChannelState)
