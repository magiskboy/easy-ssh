# Install-time Linux bundling. Expects:
#   EASY_SSH_BUNDLE_LIB_FILES — absolute .so paths
#   EASY_SSH_QT_PLUGINS_SRC   — Qt plugins root under .deps/qt (may be empty)
#   EASY_SSH_PATCHELF         — patchelf path (may be empty)
#   EASY_SSH_INSTALL_LIBDIR   — e.g. lib
#
# Use on-disk paths via easy_ssh_install_root() (DESTDIR + prefix). Helpers use
# file(COPY), which does not double-apply DESTDIR.

function(easy_ssh_bundle_runtime)
    easy_ssh_resolve_linux_exe(_exe)
    if(NOT EXISTS "${_exe}")
        message(WARNING "EasySshBundle(Linux): easy-ssh binary not found")
        return()
    endif()

    easy_ssh_install_root(_root)
    set(_libdir "${_root}/${EASY_SSH_INSTALL_LIBDIR}")
    file(MAKE_DIRECTORY "${_libdir}")

    foreach(_src IN LISTS EASY_SSH_BUNDLE_LIB_FILES)
        if(_src STREQUAL "")
            continue()
        endif()
        easy_ssh_copy_lib_with_soname("${_src}" "${_libdir}")
    endforeach()

    set(_plugins_src "${EASY_SSH_QT_PLUGINS_SRC}")
    set(_plugins_dst "${_root}/plugins")
    set(_patchelf "${EASY_SSH_PATCHELF}")
    if(_plugins_src AND EXISTS "${_plugins_src}/platforms")
        foreach(_ptype IN ITEMS
            platforms xcbglintegrations imageformats iconengines
            platforminputcontexts styles tls platformthemes generic
        )
            if(NOT EXISTS "${_plugins_src}/${_ptype}")
                continue()
            endif()
            file(MAKE_DIRECTORY "${_plugins_dst}/${_ptype}")
            file(GLOB _ptype_sos "${_plugins_src}/${_ptype}/*.so")
            foreach(_pso IN LISTS _ptype_sos)
                get_filename_component(_pbase "${_pso}" NAME)
                if(_pbase MATCHES "^libq" OR _ptype STREQUAL "tls" OR _ptype STREQUAL "styles")
                    easy_ssh_copy_if_missing("${_pso}" "${_plugins_dst}/${_ptype}")
                    if(_patchelf AND EXISTS "${_plugins_dst}/${_ptype}/${_pbase}")
                        execute_process(
                            COMMAND "${_patchelf}" --set-rpath "$ORIGIN/../../lib"
                                    "${_plugins_dst}/${_ptype}/${_pbase}"
                            ERROR_QUIET)
                    endif()
                endif()
            endforeach()
        endforeach()
    endif()

    set(_conf "${_root}/${CMAKE_INSTALL_BINDIR}/qt.conf")
    file(WRITE "${_conf}"
"[Paths]
Prefix = ..
Libraries = ${EASY_SSH_INSTALL_LIBDIR}
Plugins = plugins
")
endfunction()
