/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ITunnelSession.h"

/**
 * Dynamic / SOCKS tunnel — not implement yet.
 */
class DynamicTunnelSession final : public ITunnelSession
{
    Q_OBJECT

public:
    DynamicTunnelSession(const TunnelDefinition &def, QObject *parent = nullptr);

    QUuid id() const override { return m_def.id; }
    TunnelType type() const override { return m_def.type; }

    bool start() override;
    void stop(bool emitOff) override;
    void poll() override {}

private:
    TunnelDefinition m_def;
};
