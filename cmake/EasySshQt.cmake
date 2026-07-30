include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/EasySshVersions.cmake)

if(NOT EASY_SSH_FETCH_QT)
    return()
endif()

# Reuse an existing Qt only if it can build FetchContent deps (qtermwidget still
# find_packages Qt6LinguistTools even when BUILD_TRANSLATIONS=OFF).
find_package(Qt6 ${EASY_SSH_QT_VERSION} QUIET COMPONENTS Core LinguistTools)
if(Qt6_FOUND AND Qt6LinguistTools_FOUND)
    message(STATUS "EasySshQt: using existing Qt ${Qt6_VERSION}")
    return()
endif()
if(Qt6_FOUND)
    message(STATUS
        "EasySshQt: existing Qt ${Qt6_VERSION} lacks LinguistTools; "
        "fetching ${EASY_SSH_QT_VERSION} via aqt")
endif()

set(EASY_SSH_QT_ROOT "${CMAKE_SOURCE_DIR}/.deps/qt/${EASY_SSH_QT_VERSION}")

# Map host platform to aqtinstall host/arch tokens.
if(WIN32)
    set(_aqt_host "windows")
    set(_aqt_arch "win64_msvc2022_64")
    set(_aqt_path_suffix "${EASY_SSH_QT_VERSION}/msvc2022_64")
elseif(APPLE)
    set(_aqt_host "mac")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_aqt_arch "clang_64")
        set(_aqt_path_suffix "${EASY_SSH_QT_VERSION}/macos")
    else()
        set(_aqt_arch "clang_64")
        set(_aqt_path_suffix "${EASY_SSH_QT_VERSION}/macos")
    endif()
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
    message(FATAL_ERROR "EasySshQt: unsupported platform for aqtinstall")
endif()

set(EASY_SSH_QT_PREFIX "${EASY_SSH_QT_ROOT}/${_aqt_path_suffix}")
set(_qt_marker "${EASY_SSH_QT_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake")
set(_qt_linguist_marker
    "${EASY_SSH_QT_PREFIX}/lib/cmake/Qt6LinguistTools/Qt6LinguistToolsConfig.cmake")

if(NOT EXISTS "${_qt_marker}" OR NOT EXISTS "${_qt_linguist_marker}")
    message(STATUS "EasySshQt: installing Qt ${EASY_SSH_QT_VERSION} via aqt (${_aqt_host}/${_aqt_arch})")

    find_program(EASY_SSH_AQT aqt)
    if(EASY_SSH_AQT)
        set(_aqt_launcher "${EASY_SSH_AQT}")
        set(_aqt_launch_mode "exe")
    else()
        find_program(EASY_SSH_PYTHON3 NAMES python3 python python3.exe python.exe REQUIRED)
        execute_process(
            COMMAND ${EASY_SSH_PYTHON3} -m aqt --help
            RESULT_VARIABLE _aqt_probe
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT _aqt_probe EQUAL 0)
            message(
                FATAL_ERROR
                "EasySshQt: aqtinstall not found. Install it first, e.g.:\n"
                "  python3 -m pip install -r requirements.txt"
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
        message(FATAL_ERROR "EasySshQt: aqt install-qt failed:\n${_aqt_err}\n${_aqt_out}")
    endif()

    if(NOT EXISTS "${_qt_marker}")
        message(FATAL_ERROR "EasySshQt: Qt6Config.cmake not found at ${_qt_marker}")
    endif()
    if(NOT EXISTS "${_qt_linguist_marker}")
        message(FATAL_ERROR
            "EasySshQt: Qt6LinguistToolsConfig.cmake not found at ${_qt_linguist_marker}\n"
            "aqt install may have omitted qttools; retry after removing ${EASY_SSH_QT_ROOT}")
    endif()
else()
    message(STATUS "EasySshQt: reusing cached Qt at ${EASY_SSH_QT_PREFIX}")
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

message(STATUS "EasySshQt: CMAKE_PREFIX_PATH includes ${EASY_SSH_QT_PREFIX}")
