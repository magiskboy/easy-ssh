cmake_minimum_required(VERSION 3.21)

project(easy-ssh
    VERSION 0.1.0
    DESCRIPTION "Easy SSH — lightweight cross-platform SSH / SFTP client"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Portable install layout: look for bundled libs next to the binary.
# Include both lib and lib64 — distros differ (Debian vs Fedora).
if(NOT APPLE AND NOT WIN32)
    set(CMAKE_INSTALL_RPATH
        "$ORIGIN/../lib:$ORIGIN/../lib64:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Network
)

qt_standard_project_setup()

find_package(libssh REQUIRED)
find_package(qtermwidget6 REQUIRED)
find_package(Qt6Keychain REQUIRED)

set(EASY_SSH_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/AppSettings.cpp
    src/AppSettings.h
    src/Connection.h
    src/ConnectionStore.cpp
    src/ConnectionStore.h
    src/ConnectionModel.cpp
    src/ConnectionModel.h
    src/ConnectionFilterProxy.cpp
    src/ConnectionFilterProxy.h
    src/ErrorNotifier.cpp
    src/ErrorNotifier.h
    src/Logging.cpp
    src/Logging.h
    src/SecretStore.cpp
    src/SecretStore.h
    src/SshWorker.cpp
    src/SshWorker.h
    src/SftpTypes.h
    src/RemoteFileModel.cpp
    src/RemoteFileModel.h
    src/OpenFileTracker.cpp
    src/OpenFileTracker.h
    src/Tunnel.h
    src/TunnelStore.cpp
    src/TunnelStore.h
    src/TunnelListModel.cpp
    src/TunnelListModel.h
    src/widgets/AboutDialog.cpp
    src/widgets/AboutDialog.h
    src/widgets/ConnectionDialog.cpp
    src/widgets/ConnectionDialog.h
    src/widgets/ConnectionListWidget.cpp
    src/widgets/ConnectionListWidget.h
    src/widgets/FileExplorerWidget.cpp
    src/widgets/FileExplorerWidget.h
    src/widgets/SessionTabWidget.cpp
    src/widgets/SessionTabWidget.h
    src/widgets/SettingsDialog.cpp
    src/widgets/SettingsDialog.h
    src/widgets/ShortcutsDialog.cpp
    src/widgets/ShortcutsDialog.h
    src/widgets/TerminalSessionWidget.cpp
    src/widgets/TerminalSessionWidget.h
    src/widgets/TunnelDialog.cpp
    src/widgets/TunnelDialog.h
    src/widgets/TunnelListWidget.cpp
    src/widgets/TunnelListWidget.h
    src/widgets/WelcomeWidget.cpp
    src/widgets/WelcomeWidget.h
)

set(EASY_SSH_PLATFORM_SOURCES)
if(WIN32)
    enable_language(RC)
    list(APPEND EASY_SSH_PLATFORM_SOURCES
        resources/windows/easy-ssh.rc
    )
endif()

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE easy-ssh.icns)
    set(EASY_SSH_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/easy-ssh.icns")
    set_source_files_properties("${EASY_SSH_MACOS_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND EASY_SSH_PLATFORM_SOURCES "${EASY_SSH_MACOS_ICON}")
endif()

qt_add_executable(easy-ssh
    ${EASY_SSH_SOURCES}
    ${EASY_SSH_PLATFORM_SOURCES}
)

qt_add_resources(easy-ssh "app_icons"
    PREFIX "/icons"
    BASE resources/icons
    FILES
        resources/icons/app-256.png
)

set_target_properties(easy-ssh PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME easy-ssh
    MACOSX_BUNDLE_BUNDLE_NAME "Easy SSH"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.magiskboy.easy-ssh"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (C) Easy SSH contributors"
)

target_include_directories(easy-ssh
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(easy-ssh
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Network
        ssh
        qtermwidget6
        Qt6Keychain::Qt6Keychain
)

target_compile_definitions(easy-ssh
    PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
)

install(TARGETS easy-ssh
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    resources/linux/io.github.magiskboy.easy-ssh.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES
    resources/linux/io.github.magiskboy.easy-ssh.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)

foreach(_icon_size IN ITEMS 16 22 24 32 48 64 128 256 512)
    install(FILES
        resources/linux/icons/hicolor/${_icon_size}x${_icon_size}/apps/io.github.magiskboy.easy-ssh.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps
    )
endforeach()

qt_generate_deploy_app_script(
    TARGET easy-ssh
    OUTPUT_SCRIPT easy_ssh_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${easy_ssh_deploy_script})

# --- CPack (Windows NSIS prep; usable locally on other platforms too) ---
set(CPACK_PACKAGE_NAME "easy-ssh")
set(CPACK_PACKAGE_VENDOR "Easy SSH")
set(CPACK_PACKAGE_CONTACT "https://github.com/magiskboy/easy-ssh")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight SSH / SFTP client")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Easy SSH")
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Easy SSH")
    set(CPACK_NSIS_PACKAGE_NAME "Easy SSH")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/easy-ssh.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/easy-ssh.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\easy-ssh.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Easy SSH.lnk' '$INSTDIR\\\\bin\\\\easy-ssh.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Easy SSH.lnk'"
    )
    set(CPACK_PACKAGE_EXECUTABLES "easy-ssh;Easy SSH")
endif()

include(CPack)

message(STATUS "Easy SSH ${PROJECT_VERSION} (easy-ssh)")
message(STATUS "  Qt6:          ${Qt6_VERSION}")
message(STATUS "  libssh:       found")
message(STATUS "  QTermWidget6: found")
message(STATUS "  Qt6Keychain:  found")
