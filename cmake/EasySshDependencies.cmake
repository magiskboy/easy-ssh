include_guard(GLOBAL)

include(FetchContent)
include(${CMAKE_CURRENT_LIST_DIR}/EasySshVersions.cmake)

set(_easy_ssh_using_fetch FALSE)

# libssh find_package(ZLIB) on Windows CI/dev when not using vcpkg/system zlib.
function(_easy_ssh_ensure_zlib)
    if(ZLIB_FOUND)
        return()
    endif()
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        return()
    endif()

    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
        GIT_SHALLOW    TRUE
        # Keep zlib out of default install/CPack. zlib v1.3.1 bakes
        # CMAKE_INSTALL_PREFIX into absolute INSTALL_*_DIR destinations, which
        # NSIS rejects ("ABSOLUTE path INSTALL DESTINATION forbidden").
        EXCLUDE_FROM_ALL
    )
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    # zlib v1.3.1 uses SKIP_INSTALL_*; ZLIB_INSTALL exists only on develop.
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zlib)
    FetchContent_GetProperties(zlib SOURCE_DIR _zlib_src BINARY_DIR _zlib_bin)

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

# --- lxqt-build-tools (build-only; required when building qtermwidget from source) ---
set(_easy_ssh_need_lxqt_bt FALSE)

# --- libssh ---
set(_easy_ssh_have_libssh FALSE)
if(EASY_SSH_USE_SYSTEM_PACKAGES)
    find_package(libssh ${EASY_SSH_LIBSSH_MIN_VERSION} QUIET)
    if(libssh_FOUND)
        set(_easy_ssh_have_libssh TRUE)
    endif()
endif()

if(NOT _easy_ssh_have_libssh)
    set(_easy_ssh_using_fetch TRUE)
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
else()
    message(STATUS "EasySshDeps: libssh from system packages")
endif()

# --- QtKeychain ---
set(_easy_ssh_have_qtkeychain FALSE)
if(EASY_SSH_USE_SYSTEM_PACKAGES)
    find_package(Qt6Keychain QUIET)
    if(Qt6Keychain_FOUND)
        set(_easy_ssh_have_qtkeychain TRUE)
    endif()
endif()

if(NOT _easy_ssh_have_qtkeychain)
    set(_easy_ssh_using_fetch TRUE)
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
else()
    message(STATUS "EasySshDeps: QtKeychain from system packages")
endif()

# --- QTermWidget (+ lxqt-build-tools when fetched) ---
set(_easy_ssh_have_qtermwidget FALSE)
if(EASY_SSH_USE_SYSTEM_PACKAGES)
    find_package(qtermwidget6 QUIET)
    if(qtermwidget6_FOUND)
        set(_easy_ssh_have_qtermwidget TRUE)
    endif()
endif()

if(NOT _easy_ssh_have_qtermwidget)
    set(_easy_ssh_using_fetch TRUE)
    set(_easy_ssh_need_lxqt_bt TRUE)
endif()

if(_easy_ssh_need_lxqt_bt)
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
endif()

if(NOT _easy_ssh_have_qtermwidget)
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
else()
    message(STATUS "EasySshDeps: QTermWidget from system packages")
endif()

# Strict checks when using system packages (dev workflow).
# Skip packages already provided via FetchContent fallback (e.g. Windows debug).
if(EASY_SSH_USE_SYSTEM_PACKAGES)
    if(NOT TARGET ssh)
        find_package(libssh ${EASY_SSH_LIBSSH_MIN_VERSION} REQUIRED)
    endif()
    if(NOT TARGET qtermwidget6)
        find_package(qtermwidget6 REQUIRED)
    endif()
    if(NOT TARGET Qt6Keychain::Qt6Keychain)
        find_package(Qt6Keychain REQUIRED)
    endif()
endif()

if(_easy_ssh_using_fetch)
    message(STATUS "EasySshDeps: third-party libraries built from source (FetchContent)")
endif()

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
