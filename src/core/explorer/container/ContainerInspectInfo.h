/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/container/ContainerInfo.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/// Rich inspect snapshot for the container detail dialog (not used in the list poll).
struct ContainerInspectInfo
{
    ContainerInfo base;

    QString createdAt;
    QString startedAt;
    QString finishedAt;
    QString imageId;
    QString imageName;
    QString driver;
    QString ociRuntime;
    QString hostname;
    QString user;
    QString workingDir;
    QString entrypoint;
    QString command;
    QStringList env;
    /// Mount destination → "source (rw|ro)"
    QList<QPair<QString, QString>> mounts;
    QString ipAddress;
    QString gateway;
    QString macAddress;
    QString ports;
    int exitCode = 0;
    int restartCount = 0;
    bool oomKilled = false;
    QString stateError;
    QString labels; ///< "k=v, …" summary
};
