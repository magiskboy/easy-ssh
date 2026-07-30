include_guard(GLOBAL)

install(TARGETS easy-ssh
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Fat packages: embed in-tree FetchContent shared libs, then Qt deploy, then
# GET_RUNTIME_DEPENDENCIES for remaining runtime deps. Never install(TARGETS) on
# IMPORTED system packages.

# Non-IMPORTED shared deps built in this tree (FetchContent).
set(_easy_ssh_bundle_deps "")
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
    list(APPEND _easy_ssh_bundle_deps ${_easy_ssh_dep})
endforeach()

# macOS: copy FetchContent dylibs into the .app Frameworks *before* macdeployqt.
# Otherwise macdeployqt/GET_RUNTIME_DEPENDENCIES cannot resolve @rpath/libssh.4.dylib
# (soname) and the DMG ships without those libs.
if(APPLE AND EASY_SSH_BUNDLE_RUNTIME AND _easy_ssh_bundle_deps)
    foreach(_easy_ssh_dep IN LISTS _easy_ssh_bundle_deps)
        install(CODE "
set(_src \"$<TARGET_FILE:${_easy_ssh_dep}>\")
set(_soname \"$<TARGET_SONAME_FILE_NAME:${_easy_ssh_dep}>\")
set(_fw \"\${CMAKE_INSTALL_PREFIX}/easy-ssh.app/Contents/Frameworks\")
file(MAKE_DIRECTORY \"\${_fw}\")
file(INSTALL \"\${_src}\" DESTINATION \"\${_fw}\" RENAME \"\${_soname}\")
execute_process(COMMAND install_name_tool
  -id \"@executable_path/../Frameworks/\${_soname}\" \"\${_fw}/\${_soname}\"
  ERROR_QUIET)
file(GLOB _apps \"\${CMAKE_INSTALL_PREFIX}/*.app\")
list(LENGTH _apps _app_count)
if(_app_count GREATER 0)
  list(GET _apps 0 _app)
  file(GLOB _bins \"\${_app}/Contents/MacOS/*\")
  foreach(_candidate IN LISTS _bins)
    if(IS_DIRECTORY \"\${_candidate}\")
      continue()
    endif()
    get_filename_component(_bin \"\${_candidate}\" NAME)
    if(_bin MATCHES \"^\\\\.\\\\.\")
      continue()
    endif()
    execute_process(COMMAND install_name_tool
      -change \"@rpath/\${_soname}\" \"@executable_path/../Frameworks/\${_soname}\" \"\${_candidate}\"
      ERROR_QUIET)
    break()
  endforeach()
endif()
")
    endforeach()
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

set(_easy_ssh_deploy_tool_options "")
if(WIN32)
    set(_easy_ssh_deploy_tool_options --ignore-library-errors)
elseif(APPLE)
    # Ad-hoc sign; re-signed again after any later install_name_tool.
    # -libpath helps macdeployqt find remaining non-Qt deps in the build tree.
    set(_easy_ssh_deploy_tool_options
        -codesign=-
        -libpath=${CMAKE_BINARY_DIR}/lib
    )
endif()

qt_generate_deploy_app_script(
    TARGET easy-ssh
    OUTPUT_SCRIPT easy_ssh_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
    DEPLOY_TOOL_OPTIONS ${_easy_ssh_deploy_tool_options}
)
install(SCRIPT ${easy_ssh_deploy_script})

if(EASY_SSH_BUNDLE_RUNTIME)
    # Collect extra search paths for FetchContent build-tree libs / prefix installs.
    set(_easy_ssh_runtime_search_dirs "")
    foreach(_build_subdir IN ITEMS bin lib lib64)
        if(EXISTS "${CMAKE_BINARY_DIR}/${_build_subdir}")
            list(APPEND _easy_ssh_runtime_search_dirs "${CMAKE_BINARY_DIR}/${_build_subdir}")
        endif()
    endforeach()
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
    if(APPLE)
        foreach(_brew_lib IN ITEMS /opt/homebrew/lib /usr/local/lib)
            if(EXISTS "${_brew_lib}")
                list(APPEND _easy_ssh_runtime_search_dirs "${_brew_lib}")
            endif()
        endforeach()
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
        set(_bundled \"\${_fw}/\${_base}\")
        if(EXISTS \"\${_bundled}\")
          execute_process(COMMAND install_name_tool
            -id \"@executable_path/../Frameworks/\${_base}\" \"\${_bundled}\"
            ERROR_QUIET)
          execute_process(COMMAND install_name_tool
            -change \"\${_dep}\" \"@executable_path/../Frameworks/\${_base}\" \"\${_exe}\"
            ERROR_QUIET)
          execute_process(COMMAND install_name_tool
            -change \"@rpath/\${_base}\" \"@executable_path/../Frameworks/\${_base}\" \"\${_exe}\"
            ERROR_QUIET)
          execute_process(COMMAND install_name_tool
            -change \"/usr/local/lib/\${_base}\" \"@executable_path/../Frameworks/\${_base}\" \"\${_exe}\"
            ERROR_QUIET)
        endif()
      endif()
    endforeach()
    # @rpath/* may still appear unresolved even after we embedded FetchContent
    # libs under Frameworks — only warn if the file is truly missing.
    set(_easy_ssh_missing_unresolved \"\")
    foreach(_u IN LISTS _unresolved_deps)
      string(REGEX REPLACE \"^@rpath/\" \"\" _ubase \"\${_u}\")
      string(REGEX REPLACE \"^.*/\" \"\" _ubase \"\${_ubase}\")
      if(EXISTS \"\${_fw}/\${_ubase}\")
        execute_process(COMMAND install_name_tool
          -change \"@rpath/\${_ubase}\" \"@executable_path/../Frameworks/\${_ubase}\" \"\${_exe}\"
          ERROR_QUIET)
      else()
        list(APPEND _easy_ssh_missing_unresolved \"\${_u}\")
      endif()
    endforeach()
    if(_easy_ssh_missing_unresolved)
      message(WARNING \"Unresolved macOS runtime dependencies: \${_easy_ssh_missing_unresolved}\")
    endif()
    # install_name_tool invalidates signatures; re-sign ad-hoc for DMG/Gatekeeper.
    execute_process(
      COMMAND codesign --force --deep --sign - \"\${_app}\"
      RESULT_VARIABLE _easy_ssh_codesign_rc
      ERROR_VARIABLE _easy_ssh_codesign_err
      OUTPUT_QUIET
    )
    if(NOT _easy_ssh_codesign_rc EQUAL 0)
      message(WARNING \"EasySshInstall: codesign failed (\${_easy_ssh_codesign_rc}): \${_easy_ssh_codesign_err}\")
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
