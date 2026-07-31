# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

include_guard(GLOBAL)

include(FetchContent)
include(${CMAKE_CURRENT_LIST_DIR}/EasySshVersions.cmake)

# --- Qt (aqt binary under .deps/qt) ---

# Drop any previously cached system Qt paths so find_package cannot stick on /usr.
foreach(_easy_ssh_qt_pkg IN ITEMS
    Qt6 Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Concurrent Qt6DBus
    Qt6LinguistTools Qt6CoreTools Qt6GuiTools Qt6WidgetsTools Qt6DBusTools
)
    unset(${_easy_ssh_qt_pkg}_DIR CACHE)
endforeach()

set(EASY_SSH_QT_ROOT "${CMAKE_SOURCE_DIR}/.deps/qt/${EASY_SSH_QT_VERSION}")

# Map host platform to aqtinstall host/arch tokens.
if(WIN32)
    set(_aqt_host "windows")
    set(_aqt_arch "win64_msvc2022_64")
    set(_aqt_path_suffix "${EASY_SSH_QT_VERSION}/msvc2022_64")
elseif(APPLE)
    set(_aqt_host "mac")
    set(_aqt_arch "clang_64")
    set(_aqt_path_suffix "${EASY_SSH_QT_VERSION}/macos")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_aqt_host "linux_arm64")
        set(_aqt_arch "linux_gcc_arm64")
        set(_aqt_path_suffix "${EASY_SSH_QT_VERSION}/gcc_arm64")
    else()
        set(_aqt_host "linux")
        set(_aqt_arch "linux_gcc_64")
        set(_aqt_path_suffix "${EASY_SSH_QT_VERSION}/gcc_64")
    endif()
else()
    message(FATAL_ERROR "EasySshDeps: unsupported platform for aqtinstall")
endif()

set(EASY_SSH_QT_PREFIX "${EASY_SSH_QT_ROOT}/${_aqt_path_suffix}")
set(_qt_marker "${EASY_SSH_QT_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake")
set(_qt_linguist_marker
    "${EASY_SSH_QT_PREFIX}/lib/cmake/Qt6LinguistTools/Qt6LinguistToolsConfig.cmake")

if(NOT EXISTS "${_qt_marker}" OR NOT EXISTS "${_qt_linguist_marker}")
    message(STATUS "EasySshDeps: installing Qt ${EASY_SSH_QT_VERSION} via aqt (${_aqt_host}/${_aqt_arch})")

    # Prefer project venv from .github/scripts/install-pip-toolchains.sh (CI / local).
    set(_easy_ssh_venv_hints
        "${CMAKE_SOURCE_DIR}/.deps/venv/Scripts"
        "${CMAKE_SOURCE_DIR}/.deps/venv/bin"
    )
    find_program(EASY_SSH_AQT NAMES aqt aqt.exe HINTS ${_easy_ssh_venv_hints})
    if(EASY_SSH_AQT)
        set(_aqt_launcher "${EASY_SSH_AQT}")
        set(_aqt_launch_mode "exe")
    else()
        find_program(
            EASY_SSH_PYTHON3
            NAMES python python3 python.exe python3.exe
            HINTS ${_easy_ssh_venv_hints}
            REQUIRED
        )
        execute_process(
            COMMAND ${EASY_SSH_PYTHON3} -m aqt --help
            RESULT_VARIABLE _aqt_probe
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT _aqt_probe EQUAL 0)
            message(
                FATAL_ERROR
                "EasySshDeps: aqtinstall not found. Install it first, e.g.:\n"
                "  .github/scripts/install-pip-toolchains.sh\n"
                "  # or: python3 -m pip install -r requirements/requirements.txt"
            )
        endif()
        set(_aqt_launch_mode "module")
    endif()

    file(MAKE_DIRECTORY "${EASY_SSH_QT_ROOT}")

    # qttools → LinguistTools; icu → bundled libicui18n for Linux host tools (lrelease).
    # See https://github.com/miurahr/aqtinstall/issues/532
    set(_aqt_extra_args --archives qtbase qttools)
    if(UNIX AND NOT APPLE)
        list(APPEND _aqt_extra_args icu)
    endif()

    if(_aqt_launch_mode STREQUAL "exe")
        set(_aqt_cmd
            "${_aqt_launcher}" install-qt
            ${_aqt_host}
            desktop
            ${EASY_SSH_QT_VERSION}
            ${_aqt_arch}
            --outputdir "${EASY_SSH_QT_ROOT}"
            ${_aqt_extra_args}
        )
    else()
        set(_aqt_cmd
            ${EASY_SSH_PYTHON3} -m aqt install-qt
            ${_aqt_host}
            desktop
            ${EASY_SSH_QT_VERSION}
            ${_aqt_arch}
            --outputdir "${EASY_SSH_QT_ROOT}"
            ${_aqt_extra_args}
        )
    endif()

    execute_process(
        COMMAND ${_aqt_cmd}
        RESULT_VARIABLE _aqt_result
        OUTPUT_VARIABLE _aqt_out
        ERROR_VARIABLE _aqt_err
    )
    if(NOT _aqt_result EQUAL 0)
        message(FATAL_ERROR "EasySshDeps: aqt install-qt failed:\n${_aqt_err}\n${_aqt_out}")
    endif()

    if(NOT EXISTS "${_qt_marker}")
        message(FATAL_ERROR "EasySshDeps: Qt6Config.cmake not found at ${_qt_marker}")
    endif()
    if(NOT EXISTS "${_qt_linguist_marker}")
        message(FATAL_ERROR
            "EasySshDeps: Qt6LinguistToolsConfig.cmake not found at ${_qt_linguist_marker}\n"
            "aqt install may have omitted qttools; retry after removing ${EASY_SSH_QT_ROOT}")
    endif()
else()
    message(STATUS "EasySshDeps: reusing cached Qt at ${EASY_SSH_QT_PREFIX}")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${EASY_SSH_QT_PREFIX}")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "Qt and dependency prefixes" FORCE)

if(WIN32)
    set(_qt_bin "${EASY_SSH_QT_PREFIX}/bin")
    if(EXISTS "${_qt_bin}")
        list(PREPEND CMAKE_PROGRAM_PATH "${_qt_bin}")
        set(CMAKE_PROGRAM_PATH "${CMAKE_PROGRAM_PATH}" CACHE STRING "" FORCE)
    endif()
endif()

message(STATUS "EasySshDeps: CMAKE_PREFIX_PATH includes ${EASY_SSH_QT_PREFIX}")

find_package(Qt6 ${EASY_SSH_QT_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Network
)

# Refuse distro / Homebrew Qt — both presets must use aqt under .deps/qt.
if(NOT Qt6_DIR MATCHES "/\\.deps/qt/")
    message(FATAL_ERROR
        "EasySshDeps: refusing non-project Qt at ${Qt6_DIR}\n"
        "Expected Qt ${EASY_SSH_QT_VERSION} under ${CMAKE_SOURCE_DIR}/.deps/qt "
        "(aqtinstall).")
endif()

# --- zlib (FetchContent only; required by libssh) ---

function(_easy_ssh_ensure_zlib)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        ${EASY_SSH_ZLIB_GIT_TAG}
        GIT_SHALLOW    TRUE
        # Keep zlib out of default install/CPack. zlib bakes
        # CMAKE_INSTALL_PREFIX into absolute INSTALL_*_DIR destinations, which
        # NSIS rejects ("ABSOLUTE path INSTALL DESTINATION forbidden").
        EXCLUDE_FROM_ALL
    )
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    # zlib uses SKIP_INSTALL_*; ZLIB_INSTALL exists only on develop.
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    # libssh is a shared library; static zlib must be PIC.
    set(_easy_ssh_pic_prev "${CMAKE_POSITION_INDEPENDENT_CODE}")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_MakeAvailable(zlib)
    set(CMAKE_POSITION_INDEPENDENT_CODE "${_easy_ssh_pic_prev}")
    FetchContent_GetProperties(zlib SOURCE_DIR _zlib_src BINARY_DIR _zlib_bin)
    if(TARGET zlibstatic)
        set_property(TARGET zlibstatic PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
    if(TARGET zlib)
        set_property(TARGET zlib PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()

    # zlib renames src/zconf.h → zconf.h.included and writes build/zconf.h.
    # Consumers that only pass ZLIB_INCLUDE_DIR (e.g. libssh) need zconf.h in source.
    if(NOT EXISTS "${_zlib_src}/zconf.h")
        configure_file("${_zlib_bin}/zconf.h" "${_zlib_src}/zconf.h" COPYONLY)
    endif()

    if(TARGET ZLIB::ZLIB)
        set(_zlib_lib ZLIB::ZLIB)
    elseif(TARGET zlibstatic)
        set(_zlib_lib zlibstatic)
        if(NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        endif()
    else()
        set(_zlib_lib zlib)
        if(NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB ALIAS zlib)
        endif()
    endif()

    set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
    set(ZLIB_INCLUDE_DIR "${_zlib_src}" CACHE PATH "" FORCE)
    set(ZLIB_INCLUDE_DIRS "${_zlib_src};${_zlib_bin}" CACHE STRING "" FORCE)
    set(ZLIB_LIBRARY "${_zlib_lib}" CACHE STRING "" FORCE)
    set(ZLIB_LIBRARIES "${_zlib_lib}" CACHE STRING "" FORCE)
endfunction()

# --- libssh ---
_easy_ssh_ensure_zlib()
FetchContent_Declare(
    libssh
    GIT_REPOSITORY https://gitlab.com/libssh/libssh-mirror.git
    GIT_TAG        ${EASY_SSH_LIBSSH_GIT_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    EXCLUDE_FROM_ALL
)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(WITH_SERVER ON CACHE BOOL "" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
set(UNIT_TESTING OFF CACHE BOOL "" FORCE)
set(WITH_GSSAPI OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libssh)
if(NOT TARGET ssh)
    message(FATAL_ERROR "FetchContent libssh did not create target 'ssh'")
endif()
message(STATUS "EasySshDeps: libssh from FetchContent (${EASY_SSH_LIBSSH_GIT_TAG})")

# --- QtKeychain ---
FetchContent_Declare(
    qtkeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG        ${EASY_SSH_QTKEYCHAIN_GIT_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    EXCLUDE_FROM_ALL
)
set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(qtkeychain)
FetchContent_GetProperties(qtkeychain SOURCE_DIR _qkc_src BINARY_DIR _qkc_bin)
# Installed package uses include/qt6keychain/; build tree only exposes qtkeychain/.
set(_qkc_shim "${_qkc_bin}/include_shim")
file(MAKE_DIRECTORY "${_qkc_shim}/qt6keychain")
configure_file(
    "${_qkc_src}/qtkeychain/keychain.h"
    "${_qkc_shim}/qt6keychain/keychain.h"
    COPYONLY
)
configure_file(
    "${_qkc_bin}/qtkeychain/qkeychain_export.h"
    "${_qkc_shim}/qt6keychain/qkeychain_export.h"
    COPYONLY
)
# FetchContent exposes qt6keychain; Qt6Keychain::Qt6Keychain exists only when installed.
if(TARGET qt6keychain AND NOT TARGET Qt6Keychain::Qt6Keychain)
    target_include_directories(qt6keychain INTERFACE "$<BUILD_INTERFACE:${_qkc_shim}>")
    add_library(Qt6Keychain::Qt6Keychain ALIAS qt6keychain)
endif()
if(NOT TARGET Qt6Keychain::Qt6Keychain)
    message(FATAL_ERROR "FetchContent qtkeychain did not create Qt6Keychain::Qt6Keychain")
endif()
# windeployqt isQtModule() matches /^Qt[major]/i (e.g. qt6keychain.dll) and then
# resolves the DLL only under QT_INSTALL_BINS. --ignore-library-errors does not
# cover that failure path. Rename so the PE import is not treated as a Qt module.
# See: https://lists.qt-project.org/pipermail/development/2025-October/046617.html
if(WIN32 AND TARGET qt6keychain)
    set_target_properties(qt6keychain PROPERTIES OUTPUT_NAME "qtkeychain")
endif()
message(STATUS "EasySshDeps: QtKeychain from FetchContent (${EASY_SSH_QTKEYCHAIN_GIT_TAG})")

# --- lxqt-build-tools (build-only; required when building qtermwidget from source) ---
FetchContent_Declare(
    lxqt-build-tools
    GIT_REPOSITORY https://github.com/lxqt/lxqt-build-tools.git
    GIT_TAG        ${EASY_SSH_LXQT_BUILD_TOOLS_GIT_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(lxqt-build-tools)
FetchContent_GetProperties(lxqt-build-tools BINARY_DIR _lxqt_bt_binary_dir)
if(NOT _lxqt_bt_binary_dir)
    message(FATAL_ERROR "FetchContent lxqt-build-tools has no BINARY_DIR")
endif()
# FetchContent keeps CMAKE_PROJECT_NAME from the top-level project (easy-ssh), so
# lxqt-build-tools writes easy-ssh-config.cmake — shim lxqt2-build-tools-config.cmake.
set(_lxqt_modules "${_lxqt_bt_binary_dir}/CMakeFiles/${PROJECT_NAME}/cmake/modules")
set(_lxqt_find_modules "${_lxqt_bt_binary_dir}/CMakeFiles/${PROJECT_NAME}/cmake/find-modules")
if(NOT IS_DIRECTORY "${_lxqt_modules}")
    message(FATAL_ERROR "EasySshDeps: lxqt modules dir not found at ${_lxqt_modules}")
endif()
set(_lxqt_shim "${_lxqt_bt_binary_dir}/lxqt2-build-tools-config.cmake")
set(_lxqt_version "${_lxqt_bt_binary_dir}/lxqt2-build-tools-config-version.cmake")
file(WRITE "${_lxqt_shim}" "
set(LXQT_CMAKE_MODULES_DIR \"${_lxqt_modules}\")
set(LXQT_CMAKE_FIND_MODULES_DIR \"${_lxqt_find_modules}\")
list(APPEND CMAKE_MODULE_PATH \"\${LXQT_CMAKE_MODULES_DIR}\" \"\${LXQT_CMAKE_FIND_MODULES_DIR}\")
set(lxqt2-build-tools_FOUND TRUE)
")
file(WRITE "${_lxqt_version}" "
set(PACKAGE_VERSION \"${EASY_SSH_LXQT_BUILD_TOOLS_GIT_TAG}\")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()
")
set(lxqt2-build-tools_DIR "${_lxqt_bt_binary_dir}" CACHE PATH "" FORCE)
list(APPEND CMAKE_PREFIX_PATH "${_lxqt_bt_binary_dir}")
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "" FORCE)
message(STATUS "EasySshDeps: lxqt-build-tools from FetchContent (${EASY_SSH_LXQT_BUILD_TOOLS_GIT_TAG})")

# --- QTermWidget ---
set(_qtw_src "${CMAKE_BINARY_DIR}/_deps/qtermwidget-src")
FetchContent_Declare(
    qtermwidget
    GIT_REPOSITORY https://github.com/lxqt/qtermwidget.git
    GIT_TAG        ${EASY_SSH_QTERMWIDGET_GIT_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    EXCLUDE_FROM_ALL
    PATCH_COMMAND
        ${CMAKE_COMMAND}
            -DEASY_SSH_REPO_ROOT=${CMAKE_SOURCE_DIR}
            -DQTERMWIDGET_SOURCE_DIR=${_qtw_src}
            -P ${CMAKE_SOURCE_DIR}/cmake/patch-qtermwidget.cmake
)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(USE_UTF8PROC OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(qtermwidget)
if(NOT TARGET qtermwidget6)
    message(FATAL_ERROR "FetchContent qtermwidget did not create target 'qtermwidget6'")
endif()
message(STATUS "EasySshDeps: QTermWidget from FetchContent (${EASY_SSH_QTERMWIDGET_GIT_TAG})")

message(STATUS "EasySshDeps: third-party libraries built from source (FetchContent)")

# FetchContent shared libs: use @rpath so the build-tree app finds them under
# ${CMAKE_BINARY_DIR}/lib (see CMAKE_BUILD_RPATH in EasySshDefaults). Skip IMPORTED.
if(APPLE)
    foreach(_easy_ssh_dep IN ITEMS ssh qt6keychain qtermwidget6)
        if(NOT TARGET ${_easy_ssh_dep})
            continue()
        endif()
        get_target_property(_easy_ssh_dep_imported ${_easy_ssh_dep} IMPORTED)
        if(_easy_ssh_dep_imported)
            continue()
        endif()
        get_target_property(_easy_ssh_dep_type ${_easy_ssh_dep} TYPE)
        if(NOT _easy_ssh_dep_type STREQUAL "SHARED_LIBRARY")
            continue()
        endif()
        set_target_properties(${_easy_ssh_dep} PROPERTIES
            INSTALL_NAME_DIR "@rpath"
            BUILD_WITH_INSTALL_NAME_DIR TRUE
        )
    endforeach()
endif()

qt_standard_project_setup()
