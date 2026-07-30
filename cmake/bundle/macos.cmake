# Install-time macOS bundling. Expects:
#   EASY_SSH_BUNDLE_LIB_FILES   — absolute dylib paths
#   EASY_SSH_BUNDLE_LIB_SONAMES — matching soname basenames (parallel list)

function(easy_ssh_bundle_macos_pre_qt)
    easy_ssh_resolve_macos_app(_app _exe)
    if(_app STREQUAL "" OR _exe STREQUAL "")
        message(WARNING "EasySshBundle(macOS): no .app for pre-qt copy")
        return()
    endif()
    set(_fw "${_app}/Contents/Frameworks")
    file(MAKE_DIRECTORY "${_fw}")

    list(LENGTH EASY_SSH_BUNDLE_LIB_FILES _n)
    math(EXPR _last "${_n} - 1")
    if(_last LESS 0)
        return()
    endif()
    foreach(_i RANGE ${_last})
        list(GET EASY_SSH_BUNDLE_LIB_FILES ${_i} _src)
        list(GET EASY_SSH_BUNDLE_LIB_SONAMES ${_i} _soname)
        if(NOT EXISTS "${_src}")
            continue()
        endif()
        easy_ssh_copy_if_missing("${_src}" "${_fw}" RENAME "${_soname}")
        execute_process(COMMAND install_name_tool
            -id "@executable_path/../Frameworks/${_soname}" "${_fw}/${_soname}"
            ERROR_QUIET)
        execute_process(COMMAND install_name_tool
            -change "@rpath/${_soname}" "@executable_path/../Frameworks/${_soname}" "${_exe}"
            ERROR_QUIET)
    endforeach()
endfunction()

function(easy_ssh_bundle_runtime)
    easy_ssh_resolve_macos_app(_app _exe)
    if(_app STREQUAL "" OR _exe STREQUAL "")
        message(WARNING "EasySshBundle(macOS): no .app for runtime bundle")
        return()
    endif()
    set(_fw "${_app}/Contents/Frameworks")
    file(MAKE_DIRECTORY "${_fw}")

    list(LENGTH EASY_SSH_BUNDLE_LIB_FILES _n)
    math(EXPR _last "${_n} - 1")
    if(_last GREATER_EQUAL 0)
        foreach(_i RANGE ${_last})
            list(GET EASY_SSH_BUNDLE_LIB_FILES ${_i} _src)
            list(GET EASY_SSH_BUNDLE_LIB_SONAMES ${_i} _soname)
            if(NOT EXISTS "${_src}")
                continue()
            endif()
            easy_ssh_copy_if_missing("${_src}" "${_fw}" RENAME "${_soname}")
            set(_bundled "${_fw}/${_soname}")
            execute_process(COMMAND install_name_tool
                -id "@executable_path/../Frameworks/${_soname}" "${_bundled}"
                ERROR_QUIET)
            execute_process(COMMAND install_name_tool
                -change "${_src}" "@executable_path/../Frameworks/${_soname}" "${_exe}"
                ERROR_QUIET)
            execute_process(COMMAND install_name_tool
                -change "@rpath/${_soname}" "@executable_path/../Frameworks/${_soname}" "${_exe}"
                ERROR_QUIET)
            execute_process(COMMAND install_name_tool
                -change "/usr/local/lib/${_soname}" "@executable_path/../Frameworks/${_soname}" "${_exe}"
                ERROR_QUIET)
        endforeach()
    endif()

    # Ad-hoc re-sign after install_name_tool (invalidates prior signature).
    execute_process(
        COMMAND codesign --force --deep --sign - "${_app}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err
        OUTPUT_QUIET
    )
    if(NOT _rc EQUAL 0)
        message(WARNING "EasySshBundle(macOS): codesign failed (${_rc}): ${_err}")
    endif()
endfunction()
