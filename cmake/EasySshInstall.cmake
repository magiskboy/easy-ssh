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

# Linux fat packages: ensure QPA plugins land under prefix/plugins (same layout
# as qt_generate_deploy_app_script / CPack AppImage). Plugin RPATH is typically
# $ORIGIN/../../lib → prefix/lib. Putting them under lib/qt6/plugins breaks that
# and the loader falls back to system libQt6XcbQpa (Qt_6_*_PRIVATE_API mismatch);
# Qt then prints a misleading "need libxcb-cursor0" message.
# GRD skips plugins when Qt lives in system lib dirs (Fedora /usr/lib64); we
# always copy as a safety net so AppImage/DEB have libqxcb.
set(_easy_ssh_qt_plugins_src "")
set(_easy_ssh_patchelf "")
if(UNIX AND NOT APPLE AND EASY_SSH_BUNDLE_RUNTIME)
    foreach(_easy_ssh_plug_cand IN ITEMS
        "${Qt6_DIR}/../../../plugins"
        "${Qt6_DIR}/../../../qt6/plugins"
    )
        get_filename_component(_easy_ssh_plug_abs "${_easy_ssh_plug_cand}" ABSOLUTE)
        if(EXISTS "${_easy_ssh_plug_abs}/platforms")
            set(_easy_ssh_qt_plugins_src "${_easy_ssh_plug_abs}")
            break()
        endif()
    endforeach()
    if(NOT _easy_ssh_qt_plugins_src AND EXISTS "/usr/lib64/qt6/plugins/platforms")
        set(_easy_ssh_qt_plugins_src "/usr/lib64/qt6/plugins")
    endif()
    if(_easy_ssh_qt_plugins_src)
        message(STATUS "EasySshInstall: Qt plugins from ${_easy_ssh_qt_plugins_src}")
    else()
        message(WARNING "EasySshInstall: Qt plugins directory not found (AppImage may fail to start)")
    endif()
    find_program(EASY_SSH_PATCHELF NAMES patchelf)
    if(EASY_SSH_PATCHELF)
        set(_easy_ssh_patchelf "${EASY_SSH_PATCHELF}")
        message(STATUS "EasySshInstall: patchelf at ${EASY_SSH_PATCHELF}")
    else()
        message(WARNING "EasySshInstall: patchelf not found; CPack AppImage may break bundled libs")
    endif()
endif()

if(EASY_SSH_BUNDLE_RUNTIME)
    # Collect extra search paths for FetchContent build-tree libs / prefix installs.
    # Always include CMAKE_BINARY_DIR/{bin,lib,lib64}: EXISTS at configure time is
    # false before the first build, which left DIRECTORIES empty and made
    # ssh/qtkeychain/qtermwidget6 unresolved on Windows CI.
    set(_easy_ssh_runtime_search_dirs
        "${CMAKE_BINARY_DIR}/bin"
        "${CMAKE_BINARY_DIR}/lib"
        "${CMAKE_BINARY_DIR}/lib64"
    )
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
    if(WIN32)
        # libssh links OpenSSL; GitHub windows runners ship it under Program Files.
        foreach(_ssl_bin IN ITEMS
            "$ENV{OPENSSL_ROOT_DIR}/bin"
            "C:/Program Files/OpenSSL/bin"
            "C:/Program Files/OpenSSL-Win64/bin"
        )
            if(EXISTS "${_ssl_bin}")
                list(APPEND _easy_ssh_runtime_search_dirs "${_ssl_bin}")
            endif()
        endforeach()
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
        # Copy FetchContent DLLs next to the exe (same idea as macOS Frameworks).
        # Avoid install(TARGETS) on those deps — their own install rules may use
        # absolute destinations that NSIS rejects.
        foreach(_easy_ssh_dep IN LISTS _easy_ssh_bundle_deps)
            install(CODE "
set(_src \"$<TARGET_FILE:${_easy_ssh_dep}>\")
if(EXISTS \"\${_src}\")
  get_filename_component(_base \"\${_src}\" NAME)
  if(NOT EXISTS \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/\${_base}\")
    file(INSTALL \"\${_src}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
  endif()
endif()
")
        endforeach()

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
      \"hvsi\"
      \"wpaxholder\"
    POST_EXCLUDE_REGEXES
      \".*[\\\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\\\/].*\"
      \".*[\\\\/][Ss][Yy][Ss][Ww][Oo][Ww]64[\\\\/].*\"
      \".*[\\\\/][Ww][Ii][Nn][Ss][Xx][Ss][\\\\/].*\"
  )
  foreach(_dep IN LISTS _resolved_deps)
    string(TOLOWER \"\${_dep}\" _dep_lower)
    if(_dep_lower MATCHES \"system32|syswow64|winsxs\")
      continue()
    endif()
    get_filename_component(_base \"\${_dep}\" NAME)
    if(NOT EXISTS \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/\${_base}\")
      file(INSTALL \"\${_dep}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
    endif()
  endforeach()
  set(_easy_ssh_missing_unresolved \"\")
  foreach(_u IN LISTS _unresolved_deps)
    string(TOLOWER \"\${_u}\" _u_lower)
    if(_u_lower MATCHES \"hvsi|wpaxholder|api-ms-|ext-ms-\")
      continue()
    endif()
    if(EXISTS \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/\${_u}\")
      continue()
    endif()
    list(APPEND _easy_ssh_missing_unresolved \"\${_u}\")
  endforeach()
  if(_easy_ssh_missing_unresolved)
    message(WARNING \"Unresolved Windows runtime dependencies: \${_easy_ssh_missing_unresolved}\")
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
      \"^/usr/\"
      \"^/lib64/\"
      \"^/lib/\"
  )
  foreach(_dep IN LISTS _resolved_deps)
    get_filename_component(_base \"\${_dep}\" NAME)
    # Prefer the real file (follow soname symlinks); then add the soname link.
    file(REAL_PATH \"\${_dep}\" _real)
    get_filename_component(_real_base \"\${_real}\" NAME)
    if(NOT EXISTS \"\${_libdir}/\${_real_base}\")
      file(INSTALL \"\${_real}\" DESTINATION \"\${_libdir}\")
    endif()
    if(NOT _base STREQUAL _real_base AND NOT EXISTS \"\${_libdir}/\${_base}\")
      file(CREATE_LINK \"\${_real_base}\" \"\${_libdir}/\${_base}\" SYMBOLIC)
    endif()
  endforeach()
  # Bundle QPA / image / TLS plugins under prefix/plugins (matches GRD + CPack
  # AppImage RPATH $ORIGIN/../../lib → prefix/lib). See comment above.
  set(_plugins_src \"${_easy_ssh_qt_plugins_src}\")
  set(_plugins_dst \"\${CMAKE_INSTALL_PREFIX}/plugins\")
  set(_patchelf \"${_easy_ssh_patchelf}\")
  if(_plugins_src AND EXISTS \"\${_plugins_src}/platforms\")
    foreach(_ptype IN ITEMS
        platforms xcbglintegrations imageformats iconengines
        platforminputcontexts styles tls platformthemes generic
    )
      if(EXISTS \"\${_plugins_src}/\${_ptype}\")
        file(MAKE_DIRECTORY \"\${_plugins_dst}/\${_ptype}\")
        file(GLOB _ptype_sos \"\${_plugins_src}/\${_ptype}/*.so\")
        foreach(_pso IN LISTS _ptype_sos)
          get_filename_component(_pbase \"\${_pso}\" NAME)
          # Skip designer-/kde-heavy extras; keep libq* Qt plugins.
          if(_pbase MATCHES \"^libq\" OR _ptype STREQUAL \"tls\" OR _ptype STREQUAL \"styles\")
            file(INSTALL \"\${_pso}\" DESTINATION \"\${_plugins_dst}/\${_ptype}\")
            if(_patchelf)
              execute_process(
                COMMAND \"\${_patchelf}\" --set-rpath \"\$ORIGIN/../../lib\"
                        \"\${_plugins_dst}/\${_ptype}/\${_pbase}\"
                ERROR_QUIET)
            endif()
          endif()
        endforeach()
      endif()
    endforeach()
    file(GLOB_RECURSE _plugin_sos \"\${_plugins_dst}/*.so\")
    if(_plugin_sos)
      file(GET_RUNTIME_DEPENDENCIES
        MODULES \${_plugin_sos}
        RESOLVED_DEPENDENCIES_VAR _plugin_deps
        UNRESOLVED_DEPENDENCIES_VAR _plugin_unresolved
        DIRECTORIES
${_easy_ssh_runtime_dirs_code}        POST_EXCLUDE_REGEXES
          \"ld-linux\"
          \"libc\\\\.so\"
          \"libm\\\\.so\"
          \"libdl\\\\.so\"
          \"libpthread\\\\.so\"
          \"librt\\\\.so\"
          \"libresolv\\\\.so\"
          \"libstdc\\\\+\\\\+\"
          \"libgcc_s\\\\.so\"
          \"^/usr/\"
          \"^/lib64/\"
          \"^/lib/\"
      )
      foreach(_dep IN LISTS _plugin_deps)
        get_filename_component(_base \"\${_dep}\" NAME)
        file(REAL_PATH \"\${_dep}\" _real)
        get_filename_component(_real_base \"\${_real}\" NAME)
        if(NOT EXISTS \"\${_libdir}/\${_real_base}\")
          file(INSTALL \"\${_real}\" DESTINATION \"\${_libdir}\")
        endif()
        if(NOT _base STREQUAL _real_base AND NOT EXISTS \"\${_libdir}/\${_base}\")
          file(CREATE_LINK \"\${_real_base}\" \"\${_libdir}/\${_base}\" SYMBOLIC)
        endif()
      endforeach()
    endif()
  endif()
  set(_conf \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/qt.conf\")
  # Plugins = plugins matches GRD/CPack layout and plugin RPATH $ORIGIN/../../lib.
  file(WRITE \"\${_conf}\"
\"[Paths]
Prefix = ..
Libraries = ${CMAKE_INSTALL_LIBDIR}
Plugins = plugins
\")
  if(_unresolved_deps)
    message(WARNING \"Unresolved Linux runtime dependencies: \${_unresolved_deps}\")
  endif()
endif()
")
    endif()
endif()
