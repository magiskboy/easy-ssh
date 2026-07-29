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
    )
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zlib)

    if(TARGET ZLIB::ZLIB)
        set(_zlib_lib ZLIB::ZLIB)
    elseif(TARGET zlibstatic)
        set(_zlib_lib zlibstatic)
    else()
        set(_zlib_lib zlib)
    endif()

    set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
    set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(ZLIB_LIBRARY "${_zlib_lib}" CACHE STRING "" FORCE)
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
    )
    set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
    set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(qtkeychain)
    # FetchContent exposes qt6keychain; Qt6Keychain::Qt6Keychain exists only when installed.
    if(TARGET qt6keychain AND NOT TARGET Qt6Keychain::Qt6Keychain)
        add_library(Qt6Keychain::Qt6Keychain ALIAS qt6keychain)
    endif()
    if(NOT TARGET Qt6Keychain::Qt6Keychain)
        message(FATAL_ERROR "FetchContent qtkeychain did not create Qt6Keychain::Qt6Keychain")
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
    )
    FetchContent_MakeAvailable(lxqt-build-tools)
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
if(EASY_SSH_USE_SYSTEM_PACKAGES)
    find_package(libssh ${EASY_SSH_LIBSSH_MIN_VERSION} REQUIRED)
    find_package(qtermwidget6 REQUIRED)
    find_package(Qt6Keychain REQUIRED)
endif()

if(_easy_ssh_using_fetch)
    message(STATUS "EasySshDeps: third-party libraries built from source (FetchContent)")
endif()
