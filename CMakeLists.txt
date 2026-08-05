# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 4.2)

project(easy-ssh
    VERSION 0.1.0
    DESCRIPTION "Easy SSH — lightweight cross-platform SSH / SFTP client"
    LANGUAGES CXX
)

include(cmake/EasySshVersions.cmake)
include(cmake/EasySshOptions.cmake)
include(cmake/EasySshDefaults.cmake)
include(cmake/EasySshDependencies.cmake)
include(CTest)

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
add_subdirectory(resources)

include(cmake/EasySshInstall.cmake)
include(cmake/EasySshPackaging.cmake)

message(STATUS "Easy SSH ${PROJECT_VERSION} (easy-ssh)")
message(STATUS "  Qt6:          ${Qt6_VERSION}")
message(STATUS "  libssh:       found")
message(STATUS "  QTermWidget6: found")
message(STATUS "  Qt6Keychain:  found")
message(STATUS "  Qt ADS:       ${EASY_SSH_ADS_GIT_TAG}")
