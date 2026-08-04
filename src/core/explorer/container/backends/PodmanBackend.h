/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/container/IContainerBackend.h"

class PodmanBackend final : public IContainerBackend
{
public:
    QString runtimeId() const override;
    QString remoteListSnippet() const override;
    QString remoteStatsSnippet() const override;
    bool parseLine(const QJsonObject &object, ContainerInfo *out) const override;
};
