# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

include_guard(GLOBAL)

# Shared helpers for configure-time and install-time (via install(SCRIPT)).
# Other cmake modules may include this file:
#   include(${CMAKE_CURRENT_LIST_DIR}/EasySshHelpers.cmake)

# --- Configure-time -----------------------------------------------------------

# Append non-IMPORTED SHARED_LIBRARY targets named in ARGN to OUT_VAR.
function(easy_ssh_collect_shared_targets OUT_VAR)
    set(_result "")
    foreach(_name IN LISTS ARGN)
        if(NOT TARGET ${_name})
            continue()
        endif()
        get_target_property(_imported ${_name} IMPORTED)
        if(_imported)
            continue()
        endif()
        get_target_property(_type ${_name} TYPE)
        if(NOT _type STREQUAL "SHARED_LIBRARY")
            continue()
        endif()
        list(APPEND _result ${_name})
    endforeach()
    set(${OUT_VAR} "${_result}" PARENT_SCOPE)
endfunction()

# --- Install-time utilities ---------------------------------------------------

# Root of the install tree at install-time (honors DESTDIR for CPack).
function(easy_ssh_install_root OUT_VAR)
    set(${OUT_VAR} "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}" PARENT_SCOPE)
endfunction()

function(easy_ssh_normalize_path IN_PATH OUT_VAR)
    string(REPLACE "\\" "/" _p "${IN_PATH}")
    set(${OUT_VAR} "${_p}" PARENT_SCOPE)
endfunction()

function(easy_ssh_copy_if_missing SRC DEST_DIR)
    set(_rename "")
    cmake_parse_arguments(_arg "" "RENAME" "" ${ARGN})
    if(NOT EXISTS "${SRC}")
        message(WARNING "EasySshHelpers: missing source to copy: ${SRC}")
        return()
    endif()
    file(MAKE_DIRECTORY "${DEST_DIR}")
    if(_arg_RENAME)
        set(_dest "${DEST_DIR}/${_arg_RENAME}")
    else()
        get_filename_component(_base "${SRC}" NAME)
        set(_dest "${DEST_DIR}/${_base}")
    endif()
    if(NOT EXISTS "${_dest}")
        # file(COPY) does not prepend DESTDIR (unlike file(INSTALL)).
        if(_arg_RENAME)
            file(COPY "${SRC}" DESTINATION "${DEST_DIR}")
            get_filename_component(_src_base "${SRC}" NAME)
            file(RENAME "${DEST_DIR}/${_src_base}" "${_dest}")
        else()
            file(COPY "${SRC}" DESTINATION "${DEST_DIR}")
        endif()
    endif()
endfunction()

# Copy a shared library; if SRC is a soname symlink, install the real file and
# recreate the soname link in DEST_DIR. DEST_DIR must be the on-disk path
# (already includes DESTDIR when packaging).
function(easy_ssh_copy_lib_with_soname SRC DEST_DIR)
    if(NOT EXISTS "${SRC}")
        message(WARNING "EasySshHelpers: missing library: ${SRC}")
        return()
    endif()
    file(MAKE_DIRECTORY "${DEST_DIR}")
    get_filename_component(_base "${SRC}" NAME)
    file(REAL_PATH "${SRC}" _real)
    get_filename_component(_real_base "${_real}" NAME)
    if(NOT EXISTS "${DEST_DIR}/${_real_base}")
        file(COPY "${_real}" DESTINATION "${DEST_DIR}")
    endif()
    if(NOT _base STREQUAL _real_base AND NOT EXISTS "${DEST_DIR}/${_base}")
        file(CREATE_LINK "${_real_base}" "${DEST_DIR}/${_base}" SYMBOLIC)
    endif()
endfunction()

function(easy_ssh_copy_file_list FILE_LIST DEST_DIR)
    foreach(_src IN LISTS FILE_LIST)
        if(_src STREQUAL "")
            continue()
        endif()
        easy_ssh_copy_if_missing("${_src}" "${DEST_DIR}")
    endforeach()
endfunction()

function(easy_ssh_resolve_linux_exe OUT_VAR)
    easy_ssh_install_root(_root)
    set(_exe "${_root}/${CMAKE_INSTALL_BINDIR}/easy-ssh")
    if(NOT EXISTS "${_exe}")
        set(_exe "${_root}/easy-ssh")
    endif()
    set(${OUT_VAR} "${_exe}" PARENT_SCOPE)
endfunction()

function(easy_ssh_resolve_windows_exe OUT_VAR)
    easy_ssh_install_root(_root)
    set(_exe "${_root}/${CMAKE_INSTALL_BINDIR}/easy-ssh.exe")
    if(NOT EXISTS "${_exe}")
        set(_exe "${_root}/easy-ssh.exe")
    endif()
    set(${OUT_VAR} "${_exe}" PARENT_SCOPE)
endfunction()

# Sets OUT_APP and OUT_EXE (empty if not found). Prefers the Qt Widgets bundle.
function(easy_ssh_resolve_macos_app OUT_APP OUT_EXE)
    easy_ssh_install_root(_root)
    set(_app "")
    if(EXISTS "${_root}/easy-ssh.app")
        set(_app "${_root}/easy-ssh.app")
    else()
        file(GLOB _apps "${_root}/*.app")
        list(LENGTH _apps _app_count)
        if(_app_count GREATER 0)
            list(GET _apps 0 _app)
        endif()
    endif()
    if(_app STREQUAL "")
        set(${OUT_APP} "" PARENT_SCOPE)
        set(${OUT_EXE} "" PARENT_SCOPE)
        return()
    endif()
    set(_exe "")
    file(GLOB _bin_candidates "${_app}/Contents/MacOS/*")
    foreach(_candidate IN LISTS _bin_candidates)
        if(IS_DIRECTORY "${_candidate}")
            continue()
        endif()
        get_filename_component(_bin "${_candidate}" NAME)
        if(_bin MATCHES "^\\.\\.")
            continue()
        endif()
        set(_exe "${_candidate}")
        break()
    endforeach()
    set(${OUT_APP} "${_app}" PARENT_SCOPE)
    set(${OUT_EXE} "${_exe}" PARENT_SCOPE)
endfunction()

# Run GET_RUNTIME_DEPENDENCIES on EXE and copy resolved deps whose basename
# matches ALLOW_REGEX into DEST_DIR. DIRECTORIES is a list of search paths.
# PRE/POST exclude regexes keep OS commons out.
function(easy_ssh_grd_copy_matching EXE DEST_DIR ALLOW_REGEX)
    cmake_parse_arguments(_arg "" "" "DIRECTORIES;PRE_EXCLUDE;POST_EXCLUDE" ${ARGN})
    if(NOT EXISTS "${EXE}")
        message(WARNING "EasySshHelpers: GRD skipped; exe missing: ${EXE}")
        return()
    endif()
    file(MAKE_DIRECTORY "${DEST_DIR}")
    set(_pre ${_arg_PRE_EXCLUDE})
    set(_post ${_arg_POST_EXCLUDE})
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${EXE}"
        RESOLVED_DEPENDENCIES_VAR _resolved
        UNRESOLVED_DEPENDENCIES_VAR _unresolved
        DIRECTORIES ${_arg_DIRECTORIES}
        PRE_EXCLUDE_REGEXES ${_pre}
        POST_EXCLUDE_REGEXES ${_post}
    )
    foreach(_dep IN LISTS _resolved)
        get_filename_component(_base "${_dep}" NAME)
        if(NOT _base MATCHES "${ALLOW_REGEX}")
            continue()
        endif()
        easy_ssh_copy_if_missing("${_dep}" "${DEST_DIR}")
    endforeach()
    set(_missing "")
    foreach(_u IN LISTS _unresolved)
        get_filename_component(_ubase "${_u}" NAME)
        if(_ubase MATCHES "${ALLOW_REGEX}" AND NOT EXISTS "${DEST_DIR}/${_ubase}")
            list(APPEND _missing "${_u}")
        endif()
    endforeach()
    if(_missing)
        message(WARNING "EasySshHelpers: unresolved matching deps: ${_missing}")
    endif()
endfunction()
