set(EASY_SSH_CORE_SOURCES
    core/util/Logging.cpp
    core/util/Logging.h
    core/settings/AppSettings.cpp
    core/settings/AppSettings.h
    core/connection/Connection.h
    core/connection/ConnectionStore.cpp
    core/connection/ConnectionStore.h
    core/connection/SecretStore.cpp
    core/connection/SecretStore.h
    core/connection/SshConfigParser.cpp
    core/connection/SshConfigParser.h
    core/ssh/SshWorker.cpp
    core/ssh/SshWorker.h
    core/ssh/SftpTypes.h
    core/tunnel/Tunnel.h
    core/tunnel/TunnelStore.cpp
    core/tunnel/TunnelStore.h
)

set(EASY_SSH_GUI_SOURCES
    gui/MainWindow.cpp
    gui/MainWindow.h
    gui/ErrorNotifier.cpp
    gui/ErrorNotifier.h
    gui/models/ConnectionModel.cpp
    gui/models/ConnectionModel.h
    gui/models/ConnectionFilterProxy.cpp
    gui/models/ConnectionFilterProxy.h
    gui/models/RemoteFileModel.cpp
    gui/models/RemoteFileModel.h
    gui/models/TunnelListModel.cpp
    gui/models/TunnelListModel.h
    gui/models/OpenFileTracker.cpp
    gui/models/OpenFileTracker.h
    gui/terminal/TerminalIoBridge.h
    gui/session/SessionTabWidget.cpp
    gui/session/SessionTabWidget.h
    gui/session/TerminalSessionWidget.cpp
    gui/session/TerminalSessionWidget.h
    gui/connection/ConnectionListWidget.cpp
    gui/connection/ConnectionListWidget.h
    gui/connection/ConnectionDialog.cpp
    gui/connection/ConnectionDialog.h
    gui/connection/WelcomeWidget.cpp
    gui/connection/WelcomeWidget.h
    gui/sftp/FileExplorerWidget.cpp
    gui/sftp/FileExplorerWidget.h
    gui/tunnel/TunnelListWidget.cpp
    gui/tunnel/TunnelListWidget.h
    gui/tunnel/TunnelDialog.cpp
    gui/tunnel/TunnelDialog.h
    gui/dialogs/AboutDialog.cpp
    gui/dialogs/AboutDialog.h
    gui/dialogs/SettingsDialog.cpp
    gui/dialogs/SettingsDialog.h
    gui/dialogs/ShortcutsDialog.cpp
    gui/dialogs/ShortcutsDialog.h
)

if(WIN32)
    list(APPEND EASY_SSH_GUI_SOURCES gui/terminal/TerminalIoBridge_win.cpp)
else()
    list(APPEND EASY_SSH_GUI_SOURCES gui/terminal/TerminalIoBridge_unix.cpp)
endif()

set(EASY_SSH_PLATFORM_SOURCES)
if(WIN32)
    enable_language(RC)
    list(APPEND EASY_SSH_PLATFORM_SOURCES
        ${CMAKE_SOURCE_DIR}/resources/windows/easy-ssh.rc
    )
endif()

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE easy-ssh.icns)
    set(EASY_SSH_MACOS_ICON "${CMAKE_SOURCE_DIR}/resources/macos/easy-ssh.icns")
    set_source_files_properties("${EASY_SSH_MACOS_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND EASY_SSH_PLATFORM_SOURCES "${EASY_SSH_MACOS_ICON}")
endif()

qt_add_executable(easy-ssh
    app/main.cpp
    ${EASY_SSH_CORE_SOURCES}
    ${EASY_SSH_GUI_SOURCES}
    ${EASY_SSH_PLATFORM_SOURCES}
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
        ${CMAKE_CURRENT_SOURCE_DIR}
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
