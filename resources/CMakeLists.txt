# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

qt_add_resources(easy-ssh "app_icons"
    PREFIX "/icons"
    BASE ${CMAKE_SOURCE_DIR}/resources/icons
    FILES
        ${CMAKE_SOURCE_DIR}/resources/icons/app-256.png
)
