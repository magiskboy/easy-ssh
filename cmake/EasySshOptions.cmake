# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

include_guard(GLOBAL)

# Optional CI artifact suffix, e.g. linux-amd64 (passed from GitHub Actions matrix).
set(EASY_SSH_PACKAGE_SUFFIX "" CACHE STRING
    "Suffix for CPack output file names (e.g. linux-amd64)")

# Parallel SwiftUI + SwiftTerm shell on macOS (keeps Qt Widgets easy-ssh as fallback).
if(APPLE)
    option(EASY_SSH_NATIVE_MACOS "Build SwiftUI+SwiftTerm macOS app (easy-ssh-native)" ON)
else()
    set(EASY_SSH_NATIVE_MACOS OFF CACHE BOOL "Build SwiftUI+SwiftTerm macOS app (easy-ssh-native)" FORCE)
endif()
