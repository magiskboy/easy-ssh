/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(lcSsh)
Q_DECLARE_LOGGING_CATEGORY(lcApp)

void initLogging();
QString logFilePath();
