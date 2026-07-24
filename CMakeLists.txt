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

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Network
)

find_package(libssh REQUIRED)
find_package(qtermwidget6 REQUIRED)
find_package(Qt6Keychain REQUIRED)

add_executable(easy-ssh
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

install(TARGETS easy-ssh RUNTIME DESTINATION bin)

message(STATUS "Easy SSH ${PROJECT_VERSION} (easy-ssh)")
message(STATUS "  Qt6:          ${Qt6_VERSION}")
message(STATUS "  libssh:       found")
message(STATUS "  QTermWidget6: found")
message(STATUS "  Qt6Keychain:  found")
