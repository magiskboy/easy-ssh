set(EASY_SSH_SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    AppSettings.cpp
    AppSettings.h
    Connection.h
    ConnectionStore.cpp
    ConnectionStore.h
    ConnectionModel.cpp
    ConnectionModel.h
    ConnectionFilterProxy.cpp
    ConnectionFilterProxy.h
    SshConfigParser.cpp
    SshConfigParser.h
    ErrorNotifier.cpp
    ErrorNotifier.h
    Logging.cpp
    Logging.h
    SecretStore.cpp
    SecretStore.h
    SshWorker.cpp
    SshWorker.h
    SftpTypes.h
    RemoteFileModel.cpp
    RemoteFileModel.h
    OpenFileTracker.cpp
    OpenFileTracker.h
    Tunnel.h
    TunnelStore.cpp
    TunnelStore.h
    TunnelListModel.cpp
    TunnelListModel.h
    terminal/TerminalIoBridge.h
    widgets/AboutDialog.cpp
    widgets/AboutDialog.h
    widgets/ConnectionDialog.cpp
    widgets/ConnectionDialog.h
    widgets/ConnectionListWidget.cpp
    widgets/ConnectionListWidget.h
    widgets/FileExplorerWidget.cpp
    widgets/FileExplorerWidget.h
    widgets/SessionTabWidget.cpp
    widgets/SessionTabWidget.h
    widgets/SettingsDialog.cpp
    widgets/SettingsDialog.h
    widgets/ShortcutsDialog.cpp
    widgets/ShortcutsDialog.h
    widgets/TerminalSessionWidget.cpp
    widgets/TerminalSessionWidget.h
    widgets/TunnelDialog.cpp
    widgets/TunnelDialog.h
    widgets/TunnelListWidget.cpp
    widgets/TunnelListWidget.h
    widgets/WelcomeWidget.cpp
    widgets/WelcomeWidget.h
)

if(WIN32)
    list(APPEND EASY_SSH_SOURCES terminal/TerminalIoBridge_win.cpp)
else()
    list(APPEND EASY_SSH_SOURCES terminal/TerminalIoBridge_unix.cpp)
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
    ${EASY_SSH_SOURCES}
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
