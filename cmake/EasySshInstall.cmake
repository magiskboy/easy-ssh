include_guard(GLOBAL)

install(TARGETS easy-ssh
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Ship FetchContent shared libs with the app (before Qt deploy / windeployqt).
set(_easy_ssh_dep_runtime_targets "")
foreach(_easy_ssh_dep IN ITEMS ssh qt6keychain qtermwidget6)
    if(TARGET ${_easy_ssh_dep})
        list(APPEND _easy_ssh_dep_runtime_targets ${_easy_ssh_dep})
    endif()
endforeach()
if(_easy_ssh_dep_runtime_targets)
    install(TARGETS ${_easy_ssh_dep_runtime_targets}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

install(FILES
    ${CMAKE_SOURCE_DIR}/resources/linux/io.github.magiskboy.easy-ssh.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES
    ${CMAKE_SOURCE_DIR}/resources/linux/io.github.magiskboy.easy-ssh.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)

foreach(_icon_size IN ITEMS 16 22 24 32 48 64 128 256 512)
    install(FILES
        ${CMAKE_SOURCE_DIR}/resources/linux/icons/hicolor/${_icon_size}x${_icon_size}/apps/io.github.magiskboy.easy-ssh.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps
    )
endforeach()

qt_generate_deploy_app_script(
    TARGET easy-ssh
    OUTPUT_SCRIPT easy_ssh_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
)
install(SCRIPT ${easy_ssh_deploy_script})

if(EASY_SSH_BUNDLE_RUNTIME)
    # Collect extra search paths for FetchContent / prefix installs.
    set(_easy_ssh_runtime_search_dirs "")
    if(CMAKE_PREFIX_PATH)
        foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
            if(EXISTS "${_prefix}/bin")
                list(APPEND _easy_ssh_runtime_search_dirs "${_prefix}/bin")
            endif()
            if(EXISTS "${_prefix}/lib")
                list(APPEND _easy_ssh_runtime_search_dirs "${_prefix}/lib")
            endif()
            if(EXISTS "${_prefix}/lib64")
                list(APPEND _easy_ssh_runtime_search_dirs "${_prefix}/lib64")
            endif()
        endforeach()
    endif()
    if(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_bin "$ENV{VCPKG_ROOT}/installed/x64-windows/bin")
        if(EXISTS "${_vcpkg_bin}")
            list(APPEND _easy_ssh_runtime_search_dirs "${_vcpkg_bin}")
        endif()
    endif()

    set(_easy_ssh_runtime_dirs_code "")
    foreach(_dir IN LISTS _easy_ssh_runtime_search_dirs)
        string(REPLACE "\\" "/" _dir_fwd "${_dir}")
        string(APPEND _easy_ssh_runtime_dirs_code "      \"${_dir_fwd}\"\n")
    endforeach()

    if(WIN32)
        install(CODE "
set(_exe \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/easy-ssh.exe\")
if(NOT EXISTS \"\${_exe}\")
  set(_exe \"\${CMAKE_INSTALL_PREFIX}/easy-ssh.exe\")
endif()
if(EXISTS \"\${_exe}\")
  file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES \"\${_exe}\"
    RESOLVED_DEPENDENCIES_VAR _resolved_deps
    UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
    DIRECTORIES
${_easy_ssh_runtime_dirs_code}    PRE_EXCLUDE_REGEXES
      \"api-ms-\"
      \"ext-ms-\"
    POST_EXCLUDE_REGEXES
      \".*[/\\\\\\\\]Windows[/\\\\\\\\]System32/.*\"
      \".*[/\\\\\\\\]WinSxS/.*\"
  )
  foreach(_dep \${_resolved_deps})
    get_filename_component(_base \"\${_dep}\" NAME)
    if(NOT EXISTS \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/\${_base}\")
      file(INSTALL \"\${_dep}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
    endif()
  endforeach()
  if(_unresolved_deps)
    message(WARNING \"Unresolved Windows runtime dependencies: \${_unresolved_deps}\")
  endif()
endif()
")
    elseif(APPLE)
        install(CODE "
file(GLOB _apps \"\${CMAKE_INSTALL_PREFIX}/*.app\")
list(LENGTH _apps _app_count)
if(_app_count EQUAL 0)
  message(WARNING \"EasySshInstall: no .app bundle found for macOS runtime bundling\")
else()
  list(GET _apps 0 _app)
  file(GLOB _bin_candidates \"\${_app}/Contents/MacOS/*\")
  foreach(_candidate IN LISTS _bin_candidates)
    if(IS_DIRECTORY \"\${_candidate}\")
      continue()
    endif()
    get_filename_component(_bin \"\${_candidate}\" NAME)
    if(_bin MATCHES \"^\\\\.\\\\.\")
      continue()
    endif()
    set(_exe \"\${_candidate}\")
    break()
  endforeach()
  if(DEFINED _exe AND EXISTS \"\${_exe}\")
    set(_fw \"\${_app}/Contents/Frameworks\")
    file(MAKE_DIRECTORY \"\${_fw}\")
    file(GET_RUNTIME_DEPENDENCIES
      EXECUTABLES \"\${_exe}\"
      RESOLVED_DEPENDENCIES_VAR _resolved_deps
      UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
      DIRECTORIES
${_easy_ssh_runtime_dirs_code}      POST_EXCLUDE_REGEXES
        \".*/System/.*\"
        \".*/usr/lib/.*\"
    )
    foreach(_dep IN LISTS _resolved_deps)
      get_filename_component(_base \"\${_dep}\" NAME)
      if(_base MATCHES \"^lib(ssh|qtermwidget|Qt6|qt6keychain|qtkeychain)\"
          OR _base MATCHES \"^Qt\"
          OR _dep MATCHES \"keychain\")
        if(NOT EXISTS \"\${_fw}/\${_base}\")
          file(INSTALL \"\${_dep}\" DESTINATION \"\${_fw}\")
        endif()
      endif()
    endforeach()
    if(_unresolved_deps)
      message(WARNING \"Unresolved macOS runtime dependencies: \${_unresolved_deps}\")
    endif()
  endif()
endif()
")
    else()
        install(CODE "
set(_exe \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/easy-ssh\")
if(NOT EXISTS \"\${_exe}\")
  set(_exe \"\${CMAKE_INSTALL_PREFIX}/easy-ssh\")
endif()
if(NOT EXISTS \"\${_exe}\")
  message(WARNING \"EasySshInstall: easy-ssh binary not found for Linux bundling\")
else()
  set(_libdir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")
  if(NOT EXISTS \"\${_libdir}\")
    file(MAKE_DIRECTORY \"\${_libdir}\")
  endif()
  file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES \"\${_exe}\"
    RESOLVED_DEPENDENCIES_VAR _resolved_deps
    UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
    DIRECTORIES
${_easy_ssh_runtime_dirs_code}    POST_EXCLUDE_REGEXES
      \"ld-linux\"
      \"libc\\\\.so\"
      \"libm\\\\.so\"
      \"libdl\\\\.so\"
      \"libpthread\\\\.so\"
      \"librt\\\\.so\"
      \"libresolv\\\\.so\"
      \"libstdc\\\\+\\\\+\"
      \"libgcc_s\\\\.so\"
  )
  foreach(_dep IN LISTS _resolved_deps)
    get_filename_component(_base \"\${_dep}\" NAME)
    if(NOT EXISTS \"\${_libdir}/\${_base}\")
      file(INSTALL \"\${_dep}\" DESTINATION \"\${_libdir}\")
      execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink
        \"\${_base}\" \"\${_libdir}/\${_base}\"
        ERROR_QUIET)
    endif()
  endforeach()
  set(_conf \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/qt.conf\")
  if(NOT EXISTS \"\${_conf}\")
    file(WRITE \"\${_conf}\"
\"[Paths]
Prefix = ..
Libraries = ${CMAKE_INSTALL_LIBDIR}
Plugins = ${CMAKE_INSTALL_LIBDIR}/qt6/plugins
\")
  endif()
  if(_unresolved_deps)
    message(WARNING \"Unresolved Linux runtime dependencies: \${_unresolved_deps}\")
  endif()
endif()
")
    endif()
endif()
