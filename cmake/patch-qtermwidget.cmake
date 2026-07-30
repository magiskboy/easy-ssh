cmake_minimum_required(VERSION 4.2)

if(NOT DEFINED EASY_SSH_REPO_ROOT)
    message(FATAL_ERROR "patch-qtermwidget: EASY_SSH_REPO_ROOT is not set")
endif()
if(NOT DEFINED QTERMWIDGET_SOURCE_DIR)
    message(FATAL_ERROR "patch-qtermwidget: QTERMWIDGET_SOURCE_DIR is not set")
endif()

set(_marker "${QTERMWIDGET_SOURCE_DIR}/.easy-ssh-patches-applied")
if(EXISTS "${_marker}")
    message(STATUS "patch-qtermwidget: patches already applied")
    return()
endif()

set(_patch_dir "${EASY_SSH_REPO_ROOT}/third_party/qtermwidget-patches")
file(GLOB _patches "${_patch_dir}/*.patch")
list(SORT _patches)

if(_patches STREQUAL "")
    message(FATAL_ERROR "patch-qtermwidget: no patches found in ${_patch_dir}")
endif()

find_program(GIT_EXECUTABLE git REQUIRED)

foreach(_patch IN LISTS _patches)
    message(STATUS "patch-qtermwidget: applying ${_patch}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --whitespace=nowarn "${_patch}"
        WORKING_DIRECTORY "${QTERMWIDGET_SOURCE_DIR}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _err
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "patch-qtermwidget: failed to apply ${_patch}:\n${_err}")
    endif()
endforeach()

file(TOUCH "${_marker}")
