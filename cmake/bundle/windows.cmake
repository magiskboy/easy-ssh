# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Install-time Windows bundling. Expects (set by generated driver):
#   EASY_SSH_BUNDLE_LIB_FILES  — absolute paths of app shared libs
#   EASY_SSH_RUNTIME_SEARCH_DIRS — dirs for narrow OpenSSL GRD

function(easy_ssh_bundle_runtime)
    easy_ssh_resolve_windows_exe(_exe)
    easy_ssh_install_root(_root)
    set(_bindir "${_root}/${CMAKE_INSTALL_BINDIR}")
    file(MAKE_DIRECTORY "${_bindir}")

    easy_ssh_copy_file_list("${EASY_SSH_BUNDLE_LIB_FILES}" "${_bindir}")

    if(NOT EXISTS "${_exe}")
        message(WARNING "EasySshBundle(Windows): easy-ssh.exe not found")
        return()
    endif()

    # libssh needs OpenSSL; copy only crypto/ssl DLLs, never System32 commons.
    easy_ssh_grd_copy_matching(
        "${_exe}"
        "${_bindir}"
        "libcrypto|libssl|ssleay|libeay"
        DIRECTORIES ${EASY_SSH_RUNTIME_SEARCH_DIRS}
        PRE_EXCLUDE
            "api-ms-"
            "ext-ms-"
            "hvsi"
            "wpaxholder"
        POST_EXCLUDE
            ".*[\\\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\\\/].*"
            ".*[\\\\/][Ss][Yy][Ss][Ww][Oo][Ww]64[\\\\/].*"
            ".*[\\\\/][Ww][Ii][Nn][Ss][Xx][Ss][\\\\/].*"
    )
endfunction()
