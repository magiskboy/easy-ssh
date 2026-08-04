/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/service/IServiceBackend.h"
#include "core/explorer/service/ServiceInfo.h"

#include <QByteArray>
#include <QString>
#include <QVector>

class SystemdBackend final : public IServiceBackend
{
public:
    QString managerId() const override;
    QString remoteListSnippet() const override;
    bool parseLine(const QJsonObject &object, ServiceInfo *out) const override;

    /// Parse JSON list-units (+ optional unit-files) blob from remoteListSnippet JSON path.
    bool parseJsonInventory(const QByteArray &stdoutBytes,
                            QVector<ServiceInfo> *out,
                            QString *error = nullptr) const;
};
