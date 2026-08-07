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
set(_qt_svg_marker "${EASY_SSH_QT_PREFIX}/lib/cmake/Qt6Svg/Qt6SvgConfig.cmake")

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

    # qttools → LinguistTools; qtsvg → ADS icons; icu → bundled libicui18n for Linux host tools.
    # See https://github.com/miurahr/aqtinstall/issues/532
    set(_aqt_extra_args --archives qtbase qttools qtsvg)
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

# Existing aqt trees may predate qtsvg; install the module into the same prefix.
if(EASY_SSH_BUILD_QT_WIDGETS AND EXISTS "${_qt_marker}" AND NOT EXISTS "${_qt_svg_marker}")
    message(STATUS "EasySshDeps: installing Qt Svg into ${EASY_SSH_QT_PREFIX}")
    set(_easy_ssh_venv_hints
        "${CMAKE_SOURCE_DIR}/.deps/venv/Scripts"
        "${CMAKE_SOURCE_DIR}/.deps/venv/bin"
    )
    find_program(EASY_SSH_AQT_SVG NAMES aqt aqt.exe HINTS ${_easy_ssh_venv_hints})
    if(EASY_SSH_AQT_SVG)
        set(_aqt_svg_cmd
            "${EASY_SSH_AQT_SVG}" install-qt
            ${_aqt_host} desktop ${EASY_SSH_QT_VERSION} ${_aqt_arch}
            --outputdir "${EASY_SSH_QT_ROOT}"
            --archives qtsvg
        )
    else()
        find_program(
            EASY_SSH_PYTHON3_SVG
            NAMES python python3 python.exe python3.exe
            HINTS ${_easy_ssh_venv_hints}
            REQUIRED
        )
        set(_aqt_svg_cmd
            ${EASY_SSH_PYTHON3_SVG} -m aqt install-qt
            ${_aqt_host} desktop ${EASY_SSH_QT_VERSION} ${_aqt_arch}
            --outputdir "${EASY_SSH_QT_ROOT}"
            --archives qtsvg
        )
    endif()
    if(UNIX AND NOT APPLE)
        list(APPEND _aqt_svg_cmd icu)
    endif()
    execute_process(
        COMMAND ${_aqt_svg_cmd}
        RESULT_VARIABLE _aqt_svg_result
        OUTPUT_VARIABLE _aqt_svg_out
        ERROR_VARIABLE _aqt_svg_err
    )
    if(NOT _aqt_svg_result EQUAL 0 OR NOT EXISTS "${_qt_svg_marker}")
        message(FATAL_ERROR
            "EasySshDeps: aqt install qtsvg failed:\n${_aqt_svg_err}\n${_aqt_svg_out}")
    endif()
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

set(_easy_ssh_qt_components Core Gui Network)
if(EASY_SSH_BUILD_QT_WIDGETS)
    list(APPEND _easy_ssh_qt_components Widgets Concurrent Svg)
endif()
find_package(Qt6 ${EASY_SSH_QT_VERSION} REQUIRED COMPONENTS ${_easy_ssh_qt_components})
unset(_easy_ssh_qt_components)

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
# Temporarily disable testing so qtkeychain does not pull its own tests.
set(_easy_ssh_had_build_testing FALSE)
if(DEFINED BUILD_TESTING)
    set(_easy_ssh_had_build_testing TRUE)
    set(_easy_ssh_build_testing "${BUILD_TESTING}")
endif()
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(qtkeychain)
if(_easy_ssh_had_build_testing)
    set(BUILD_TESTING "${_easy_ssh_build_testing}" CACHE BOOL "" FORCE)
else()
    unset(BUILD_TESTING CACHE)
endif()
unset(_easy_ssh_had_build_testing)
unset(_easy_ssh_build_testing)
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

if(EASY_SSH_BUILD_QT_WIDGETS)
    # --- lxqt-build-tools (vendored; build-only for QTermWidget) ---
    set(_lxqt_bt_src "${CMAKE_SOURCE_DIR}/third_party/lxqt-build-tools")
    if(NOT EXISTS "${_lxqt_bt_src}/CMakeLists.txt")
        message(FATAL_ERROR "EasySshDeps: missing vendored lxqt-build-tools at ${_lxqt_bt_src}")
    endif()
    FetchContent_Declare(
        lxqt-build-tools
        SOURCE_DIR ${_lxqt_bt_src}
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
    message(STATUS "EasySshDeps: lxqt-build-tools from third_party (upstream ${EASY_SSH_LXQT_BUILD_TOOLS_GIT_TAG})")

    # --- QTermWidget (vendored under third_party/qtermwidget; based on tag above) ---
    set(_qtw_src "${CMAKE_SOURCE_DIR}/third_party/qtermwidget")
    if(NOT EXISTS "${_qtw_src}/CMakeLists.txt")
        message(FATAL_ERROR "EasySshDeps: missing vendored QTermWidget at ${_qtw_src}")
    endif()
    FetchContent_Declare(
        qtermwidget
        SOURCE_DIR ${_qtw_src}
        EXCLUDE_FROM_ALL
    )
    set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
    set(USE_UTF8PROC OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(qtermwidget)
    if(NOT TARGET qtermwidget6)
        message(FATAL_ERROR "FetchContent qtermwidget did not create target 'qtermwidget6'")
    endif()
    message(STATUS "EasySshDeps: QTermWidget from third_party (upstream ${EASY_SSH_QTERMWIDGET_GIT_TAG})")

    # Bundle QTermWidget color schemes + kb-layouts next to the build/install prefix so
    # the app finds them via ../share/easy-ssh from bin/ (see third_party/qtermwidget/EASY_SSH.md).
    set(EASY_SSH_QTERMWIDGET_COLORSCHEMES_SRC "${_qtw_src}/lib/color-schemes")
    set(EASY_SSH_QTERMWIDGET_KB_LAYOUTS_SRC "${_qtw_src}/lib/kb-layouts")
    set(EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR "${CMAKE_BINARY_DIR}/share/easy-ssh")
    if(NOT EXISTS "${EASY_SSH_QTERMWIDGET_COLORSCHEMES_SRC}")
        message(FATAL_ERROR "EasySshDeps: missing QTermWidget color-schemes at ${EASY_SSH_QTERMWIDGET_COLORSCHEMES_SRC}")
    endif()
    if(NOT EXISTS "${EASY_SSH_QTERMWIDGET_KB_LAYOUTS_SRC}")
        message(FATAL_ERROR "EasySshDeps: missing QTermWidget kb-layouts at ${EASY_SSH_QTERMWIDGET_KB_LAYOUTS_SRC}")
    endif()
    file(MAKE_DIRECTORY "${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}/color-schemes")
    file(MAKE_DIRECTORY "${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}/kb-layouts")
    file(GLOB _easy_ssh_qtw_schemes "${EASY_SSH_QTERMWIDGET_COLORSCHEMES_SRC}/*.colorscheme")
    file(COPY ${_easy_ssh_qtw_schemes} DESTINATION "${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}/color-schemes")
    file(GLOB _easy_ssh_qtw_keytabs "${EASY_SSH_QTERMWIDGET_KB_LAYOUTS_SRC}/*.keytab")
    file(COPY ${_easy_ssh_qtw_keytabs} DESTINATION "${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}/kb-layouts")
    if(EXISTS "${EASY_SSH_QTERMWIDGET_KB_LAYOUTS_SRC}/historic")
        file(COPY "${EASY_SSH_QTERMWIDGET_KB_LAYOUTS_SRC}/historic"
             DESTINATION "${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}/kb-layouts")
    endif()
    message(STATUS "EasySshDeps: QTermWidget data → ${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}")

    # --- Qt Advanced Docking System ---
    set(ADS_VERSION "${EASY_SSH_ADS_VERSION}" CACHE STRING "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        qtads
        GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
        GIT_TAG        ${EASY_SSH_ADS_GIT_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(qtads)
    if(NOT TARGET qtadvanceddocking-qt6 AND NOT TARGET ads::qtadvanceddocking-qt6)
        message(FATAL_ERROR "FetchContent qtads did not create qtadvanceddocking-qt6")
    endif()
    # ADS defaults to ${CMAKE_BINARY_DIR}/x64/{lib,bin}; align with project layout.
    if(TARGET qtadvanceddocking-qt6)
        if(WIN32)
            set_target_properties(qtadvanceddocking-qt6 PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            )
        else()
            set_target_properties(qtadvanceddocking-qt6 PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            )
        endif()
    endif()
    if(TARGET qtadvanceddocking-qt6 AND NOT TARGET ads::qtadvanceddocking-qt6)
        add_library(ads::qtadvanceddocking-qt6 ALIAS qtadvanceddocking-qt6)
    endif()
    message(STATUS "EasySshDeps: Qt ADS from FetchContent (${EASY_SSH_ADS_GIT_TAG})")
else()
    message(STATUS "EasySshDeps: skipping QTermWidget / Qt ADS (Qt Widgets UI disabled)")
endif()

message(STATUS "EasySshDeps: third-party libraries built from source (FetchContent)")

# FetchContent shared libs: use @rpath so the build-tree app finds them under
# ${CMAKE_BINARY_DIR}/lib (see CMAKE_BUILD_RPATH in EasySshDefaults). Skip IMPORTED.
if(APPLE)
    foreach(_easy_ssh_dep IN ITEMS ssh qt6keychain qtermwidget6 qtadvanceddocking-qt6)
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
