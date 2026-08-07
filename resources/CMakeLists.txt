# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Qt resource embedding is only used by the Widgets shell.
if(EASY_SSH_BUILD_QT_WIDGETS)
    qt_add_resources(easy-ssh "app_icons"
        PREFIX "/icons"
        BASE ${CMAKE_SOURCE_DIR}/resources/icons
        FILES
            ${CMAKE_SOURCE_DIR}/resources/icons/app-256.png
    )
endif()

# App chrome themes (JSON palettes from qt-themes, MIT) → share/easy-ssh/themes
set(EASY_SSH_THEMES_SRC "${CMAKE_SOURCE_DIR}/resources/themes")
set(EASY_SSH_THEMES_BUILD_DIR "${CMAKE_BINARY_DIR}/share/easy-ssh/themes")
file(MAKE_DIRECTORY "${EASY_SSH_THEMES_BUILD_DIR}")
file(GLOB _easy_ssh_themes CONFIGURE_DEPENDS "${EASY_SSH_THEMES_SRC}/*.json")
if(_easy_ssh_themes)
    file(COPY ${_easy_ssh_themes} DESTINATION "${EASY_SSH_THEMES_BUILD_DIR}")
endif()
message(STATUS "Easy SSH themes → ${EASY_SSH_THEMES_BUILD_DIR}")
