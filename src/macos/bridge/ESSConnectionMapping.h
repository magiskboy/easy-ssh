/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// ObjC++ only — do not import from the Swift bridging header.

#import "ESSConnectionStore.h"

#include "core/connection/Connection.h"

Connection essConnectionFromInfo(ESSConnectionInfo *_Nullable info);
ESSConnectionInfo *_Nonnull essConnectionToInfo(const Connection &c);
SessionCredentials essCredentialsFromInfo(ESSSessionCredentials *_Nullable creds);
