# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

set(EASY_SSH_CORE_SOURCES
    core/util/Logging.cpp
    core/util/Logging.h
    core/util/FuzzyMatch.h
    core/settings/AppSettings.cpp
    core/settings/AppSettings.h
    core/connection/Connection.h
    core/connection/ConnectionStore.cpp
    core/connection/ConnectionStore.h
    core/connection/ConnectionQuery.cpp
    core/connection/ConnectionQuery.h
    core/connection/SecretStore.cpp
    core/connection/SecretStore.h
    core/connection/SshConfigParser.cpp
    core/connection/SshConfigParser.h
    core/ssh/SshWorker.cpp
    core/ssh/SshWorker.h
    core/ssh/AgentForwardHost.cpp
    core/ssh/AgentForwardHost.h
    core/ssh/SshKnownHosts.cpp
    core/ssh/SshKnownHosts.h
    core/ssh/SshAuth.cpp
    core/ssh/SshAuth.h
    core/ssh/SshSession.cpp
    core/ssh/SshSession.h
    core/session/SessionTypes.h
    core/session/Session.cpp
    core/session/Session.h
    core/session/SessionRemoteExec.cpp
    core/session/SessionRemoteExec.h
    core/session/SessionManager.cpp
    core/session/SessionManager.h
    core/session/WorkspaceState.cpp
    core/session/WorkspaceState.h
    core/shell/SshShell.cpp
    core/shell/SshShell.h
    core/fs/FsEngine.h
    core/fs/FsRemote.cpp
    core/fs/FsRemote.h
    core/fs/SftpEngine.cpp
    core/fs/SftpEngine.h
    core/fs/ScpEngine.cpp
    core/fs/ScpEngine.h
    core/fs/TransferTypes.h
    core/fs/TransferJobStore.cpp
    core/fs/TransferJobStore.h
    core/fs/ShellCommandSet.cpp
    core/fs/ShellCommandSet.h
    core/fs/ShellExecRunner.cpp
    core/fs/ShellExecRunner.h
    core/fs/Symlink.cpp
    core/fs/Symlink.h
    core/fs/SftpTypes.h
    core/explorer/ExplorerTypes.h
    core/explorer/IExplorerSource.h
    core/explorer/IRemoteExec.h
    core/explorer/process/ProcessInfo.h
    core/explorer/process/ProcessParser.cpp
    core/explorer/process/ProcessParser.h
    core/explorer/process/ProcessSource.cpp
    core/explorer/process/ProcessSource.h
    core/explorer/container/ContainerInfo.h
    core/explorer/container/ContainerInspectInfo.h
    core/explorer/container/IContainerBackend.h
    core/explorer/container/ContainerParser.cpp
    core/explorer/container/ContainerParser.h
    core/explorer/container/ContainerSource.cpp
    core/explorer/container/ContainerSource.h
    core/explorer/container/backends/PodmanBackend.cpp
    core/explorer/container/backends/PodmanBackend.h
    core/explorer/container/backends/DockerBackend.cpp
    core/explorer/container/backends/DockerBackend.h
    core/explorer/container/backends/ContainerdBackend.cpp
    core/explorer/container/backends/ContainerdBackend.h
    core/explorer/service/ServiceInfo.h
    core/explorer/service/ServiceInspectInfo.h
    core/explorer/service/IServiceBackend.h
    core/explorer/service/ServiceParser.cpp
    core/explorer/service/ServiceParser.h
    core/explorer/service/ServiceSource.cpp
    core/explorer/service/ServiceSource.h
    core/explorer/service/backends/SystemdBackend.cpp
    core/explorer/service/backends/SystemdBackend.h
    core/explorer/systeminfo/SystemInfo.h
    core/explorer/systeminfo/SystemInfoParser.cpp
    core/explorer/systeminfo/SystemInfoParser.h
    core/explorer/systeminfo/SystemInfoSource.cpp
    core/explorer/systeminfo/SystemInfoSource.h
    core/tunnel/Tunnel.h
    core/tunnel/TunnelStore.cpp
    core/tunnel/TunnelStore.h
    core/tunnel/ITunnelSession.h
    core/tunnel/TunnelBridge.cpp
    core/tunnel/TunnelBridge.h
    core/tunnel/LocalTunnelSession.cpp
    core/tunnel/LocalTunnelSession.h
    core/tunnel/RemoteTunnelSession.cpp
    core/tunnel/RemoteTunnelSession.h
    core/tunnel/DynamicTunnelSession.cpp
    core/tunnel/DynamicTunnelSession.h
    core/tunnel/Socks5Handshake.cpp
    core/tunnel/Socks5Handshake.h
    core/tunnel/TunnelSessionFactory.cpp
)

set(EASY_SSH_GUI_SOURCES
    gui/MainWindow.cpp
    gui/MainWindow.h
    gui/tray/TrayController.cpp
    gui/tray/TrayController.h
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
    gui/terminal/QTermWidgetResources.cpp
    gui/terminal/QTermWidgetResources.h
    gui/session/SessionTabWidget.cpp
    gui/session/SessionTabWidget.h
    gui/session/TerminalSessionWidget.cpp
    gui/session/TerminalSessionWidget.h
    gui/session/SessionSideBar.cpp
    gui/session/SessionSideBar.h
    gui/session/SessionPage.cpp
    gui/session/SessionPage.h
    gui/session/ShellDockHost.cpp
    gui/session/ShellDockHost.h
    gui/session/ShellLayoutPlanner.cpp
    gui/session/ShellLayoutPlanner.h
    gui/connection/ConnectionListWidget.cpp
    gui/connection/ConnectionListWidget.h
    gui/connection/ConnectionDialog.cpp
    gui/connection/ConnectionDialog.h
    gui/connection/ConnectionEditor.cpp
    gui/connection/ConnectionEditor.h
    gui/connection/ConnectionManagerDialog.cpp
    gui/connection/ConnectionManagerDialog.h
    gui/connection/ConnectionSecretHelper.cpp
    gui/connection/ConnectionSecretHelper.h
    gui/connection/WelcomeWidget.cpp
    gui/connection/WelcomeWidget.h
    gui/explorer/FileExplorerWidget.cpp
    gui/explorer/FileExplorerWidget.h
    gui/explorer/ExplorerTableModel.cpp
    gui/explorer/ExplorerTableModel.h
    gui/explorer/ExplorerPageWidget.cpp
    gui/explorer/ExplorerPageWidget.h
    gui/explorer/IExplorerModule.h
    gui/explorer/process/ProcessTableModel.cpp
    gui/explorer/process/ProcessTableModel.h
    gui/explorer/process/ProcessExplorerModule.cpp
    gui/explorer/process/ProcessExplorerModule.h
    gui/explorer/process/ProcessDetailFactory.cpp
    gui/explorer/process/ProcessDetailFactory.h
    gui/explorer/container/ContainerTableModel.cpp
    gui/explorer/container/ContainerTableModel.h
    gui/explorer/container/ContainerExplorerModule.cpp
    gui/explorer/container/ContainerExplorerModule.h
    gui/explorer/container/ContainerDetailFactory.cpp
    gui/explorer/container/ContainerDetailFactory.h
    gui/explorer/container/ContainerFilterBar.cpp
    gui/explorer/container/ContainerFilterBar.h
    gui/explorer/service/ServiceTableModel.cpp
    gui/explorer/service/ServiceTableModel.h
    gui/explorer/service/ServiceExplorerModule.cpp
    gui/explorer/service/ServiceExplorerModule.h
    gui/explorer/service/ServiceDetailFactory.cpp
    gui/explorer/service/ServiceDetailFactory.h
    gui/explorer/service/ServiceLogsDialog.cpp
    gui/explorer/service/ServiceLogsDialog.h
    gui/explorer/service/ServiceFilterBar.cpp
    gui/explorer/service/ServiceFilterBar.h
    gui/explorer/systeminfo/SystemInfoWidget.cpp
    gui/explorer/systeminfo/SystemInfoWidget.h
    gui/tunnel/TunnelListWidget.cpp
    gui/tunnel/TunnelListWidget.h
    gui/tunnel/TunnelDialog.cpp
    gui/tunnel/TunnelDialog.h
    gui/dialogs/AboutDialog.cpp
    gui/dialogs/AboutDialog.h
    gui/dialogs/CommandPaletteDialog.cpp
    gui/dialogs/CommandPaletteDialog.h
    gui/dialogs/CustomPaletteDialog.cpp
    gui/dialogs/CustomPaletteDialog.h
    gui/dialogs/SettingsDialog.cpp
    gui/dialogs/SettingsDialog.h
    gui/theme/Theme.cpp
    gui/theme/Theme.h
    gui/theme/ThemeManager.cpp
    gui/theme/ThemeManager.h
    gui/widgets/CategoryDialogShell.cpp
    gui/widgets/CategoryDialogShell.h
    gui/widgets/ExplorerColumn.h
    gui/widgets/ExplorerFilterProxy.cpp
    gui/widgets/ExplorerFilterProxy.h
    gui/widgets/ExplorerListWidget.cpp
    gui/widgets/ExplorerListWidget.h
    gui/widgets/IExplorerDetailFactory.h
    gui/widgets/UiHelpers.cpp
    gui/widgets/UiHelpers.h
    gui/widgets/UiMetrics.h
)

if(WIN32)
    list(APPEND EASY_SSH_GUI_SOURCES gui/terminal/TerminalIoBridge_win.cpp)
else()
    list(APPEND EASY_SSH_GUI_SOURCES gui/terminal/TerminalIoBridge_unix.cpp)
endif()

set(EASY_SSH_PLATFORM_SOURCES)
if(EASY_SSH_BUILD_QT_WIDGETS AND WIN32)
    enable_language(RC)
    # Absolute path so the generated .rc (in the build tree) can find the icon.
    file(TO_CMAKE_PATH
        "${CMAKE_SOURCE_DIR}/resources/windows/easy-ssh.ico"
        EASY_SSH_WINDOWS_ICON
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/windows/easy-ssh.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/easy-ssh.rc"
        @ONLY
    )
    list(APPEND EASY_SSH_PLATFORM_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/easy-ssh.rc"
    )
endif()

if(EASY_SSH_BUILD_QT_WIDGETS AND APPLE)
    set(MACOSX_BUNDLE_ICON_FILE easy-ssh.icns)
    set(EASY_SSH_MACOS_ICON "${CMAKE_SOURCE_DIR}/resources/macos/easy-ssh.icns")
    set_source_files_properties("${EASY_SSH_MACOS_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND EASY_SSH_PLATFORM_SOURCES "${EASY_SSH_MACOS_ICON}")
endif()

add_library(easy-ssh-core STATIC ${EASY_SSH_CORE_SOURCES})
target_include_directories(easy-ssh-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(easy-ssh-core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        ssh
        Qt6Keychain::Qt6Keychain
)

if(EASY_SSH_BUILD_QT_WIDGETS)
    add_library(easy-ssh-gui STATIC ${EASY_SSH_GUI_SOURCES})
    target_link_libraries(easy-ssh-gui
        PUBLIC
            easy-ssh-core
            Qt6::Widgets
            Qt6::Svg
            qtermwidget6
            ads::qtadvanceddocking-qt6
    )
    target_compile_definitions(easy-ssh-gui
        PUBLIC
            APP_VERSION="${PROJECT_VERSION}"
    )

    qt_add_executable(easy-ssh
        app/main.cpp
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

    target_link_libraries(easy-ssh
        PRIVATE
            easy-ssh-gui
    )

    # Dev builds: keep QTermWidget data next to the binary layout (bin/../share/easy-ssh).
    # On macOS also mirror into the .app Resources tree that qtermwidget searches.
    if(DEFINED EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR)
        if(APPLE)
            add_custom_command(TARGET easy-ssh POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_CONTENT_DIR:easy-ssh>/Resources/color-schemes"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_CONTENT_DIR:easy-ssh>/Resources/kb-layouts"
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_CONTENT_DIR:easy-ssh>/Resources/themes"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}/color-schemes"
                    "$<TARGET_BUNDLE_CONTENT_DIR:easy-ssh>/Resources/color-schemes"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${EASY_SSH_QTERMWIDGET_DATA_BUILD_DIR}/kb-layouts"
                    "$<TARGET_BUNDLE_CONTENT_DIR:easy-ssh>/Resources/kb-layouts"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_BINARY_DIR}/share/easy-ssh/themes"
                    "$<TARGET_BUNDLE_CONTENT_DIR:easy-ssh>/Resources/themes"
                COMMENT "Copy QTermWidget data and themes into app bundle Resources"
            )
        endif()
    endif()
endif()

if(EASY_SSH_NATIVE_MACOS)
    add_subdirectory(macos)
endif()
