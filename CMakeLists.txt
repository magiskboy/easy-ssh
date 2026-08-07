cmake_minimum_required(VERSION 4.2)

project(easy-ssh
    VERSION 0.1.0
    DESCRIPTION "Easy SSH — lightweight cross-platform SSH / SFTP client"
    LANGUAGES CXX
)

include(cmake/EasySshVersions.cmake)
include(cmake/EasySshOptions.cmake)
include(cmake/EasySshDefaults.cmake)
include(cmake/EasySshDependencies.cmake)
include(CTest)

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
add_subdirectory(resources)

include(cmake/EasySshInstall.cmake)
include(cmake/EasySshPackaging.cmake)

message(STATUS "Easy SSH ${PROJECT_VERSION}")
message(STATUS "  Qt6:          ${Qt6_VERSION}")
message(STATUS "  libssh:       found")
if(EASY_SSH_BUILD_QT_WIDGETS)
    message(STATUS "  Qt Widgets:   ON (easy-ssh)")
    message(STATUS "  QTermWidget6: found")
    message(STATUS "  Qt ADS:       ${EASY_SSH_ADS_GIT_TAG}")
else()
    message(STATUS "  Qt Widgets:   OFF")
endif()
message(STATUS "  Qt6Keychain:  found")
if(EASY_SSH_NATIVE_MACOS)
    message(STATUS "  Native macOS: ON (easy-ssh-native / SwiftUI + SwiftTerm)")
elseif(APPLE)
    message(STATUS "  Native macOS: OFF")
endif()
