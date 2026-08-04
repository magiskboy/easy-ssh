# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

include(CTest)

enable_testing()

find_package(Qt6 ${EASY_SSH_QT_VERSION} REQUIRED COMPONENTS Test)

add_subdirectory(core)
