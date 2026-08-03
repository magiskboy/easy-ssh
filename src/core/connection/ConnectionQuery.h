/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "core/connection/Connection.h"

#include <QString>

namespace ConnectionQuery
{

/// Parse a quick-connect query into draft Connection fields (id left null).
/// Supports: user@host:port, user@host, host:port, bare name/host.
Connection draftFromQuery(const QString &query);

} // namespace ConnectionQuery
