/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Connection.h"

#include <QList>

class ConnectionStore
{
public:
    static QList<Connection> load();
    static void save(const QList<Connection> &connections);
};
