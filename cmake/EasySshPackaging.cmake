include_guard(GLOBAL)

set(CPACK_PACKAGE_NAME "easy-ssh")
set(CPACK_PACKAGE_VENDOR "Easy SSH")
set(CPACK_PACKAGE_CONTACT "https://github.com/magiskboy/easy-ssh")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight SSH / SFTP client")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Easy SSH")
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
# NSIS maps CPACK_PACKAGE_ICON → MUI_HEADERIMAGE_BITMAP, which requires a BMP
# (not PNG) and often fails with forward-slash paths. Skip on Windows; MUI
# icons below are enough.
if(NOT WIN32)
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/resources/icons/app-256.png")
endif()

if(EASY_SSH_PACKAGE_SUFFIX)
    set(CPACK_PACKAGE_FILE_NAME "easy-ssh-${EASY_SSH_PACKAGE_SUFFIX}")
else()
    set(CPACK_PACKAGE_FILE_NAME "easy-ssh-${PROJECT_VERSION}")
endif()

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Easy SSH")
    set(CPACK_NSIS_PACKAGE_NAME "Easy SSH")
    # NSIS File paths need backslashes; CMake string uses \\\\ for each \.
    string(REPLACE "/" "\\\\" _easy_ssh_nsis_ico
        "${CMAKE_SOURCE_DIR}/resources/windows/easy-ssh.ico")
    set(CPACK_NSIS_MUI_ICON "${_easy_ssh_nsis_ico}")
    set(CPACK_NSIS_MUI_UNIICON "${_easy_ssh_nsis_ico}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\easy-ssh.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Easy SSH.lnk' '$INSTDIR\\\\bin\\\\easy-ssh.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Easy SSH.lnk'"
    )
    set(CPACK_PACKAGE_EXECUTABLES "easy-ssh;Easy SSH")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Easy SSH ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
    set(CPACK_PACKAGE_EXECUTABLES "easy-ssh;Easy SSH")
else()
    # AppImage is built with external appimagetool (needs CMake 4.2+ for CPack).
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_NAME "easy-ssh")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/magiskboy/easy-ssh")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    set(CPACK_RPM_PACKAGE_LICENSE "Unknown")
    set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
endif()

include(CPack)
