/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/explorer/service/ServiceInfo.h"

#include <QString>

struct ServiceInspectInfo
{
    ServiceInfo base;

    QString fragmentPath;
    QString activeEnterTimestamp;
    QString execMainStartTimestamp;
    QString type;
    QString restart;
    bool remainAfterExit = false;
};
