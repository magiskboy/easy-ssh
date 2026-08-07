/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/// Narrow remote-exec surface used by explorer *Source classes.
/// Session (Qt) and the macOS bridge each provide an implementation.
class IRemoteExec : public QObject
{
    Q_OBJECT

public:
    explicit IRemoteExec(QObject *parent = nullptr) : QObject(parent) {}
    ~IRemoteExec() override = default;

    /// One-shot remote exec (no PTY). @p requestId is echoed in commandFinished.
    virtual void execCommand(const QString &requestId, const QString &command) = 0;

signals:
    void commandFinished(const QString &requestId,
                         int exitStatus,
                         const QByteArray &stdoutBytes,
                         const QByteArray &stderrBytes,
                         const QString &errorMessage);
};
