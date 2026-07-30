cmake_minimum_required(VERSION 3.28)

project(easy-ssh
    VERSION 0.1.0
    DESCRIPTION "Easy SSH — lightweight cross-platform SSH / SFTP client"
    LANGUAGES CXX
)

include(cmake/EasySshVersions.cmake)
include(cmake/EasySshOptions.cmake)
include(cmake/EasySshDefaults.cmake)
include(cmake/EasySshDependencies.cmake)

add_subdirectory(src)
add_subdirectory(resources)

include(cmake/EasySshInstall.cmake)
include(cmake/EasySshPackaging.cmake)

message(STATUS "Easy SSH ${PROJECT_VERSION} (easy-ssh)")
message(STATUS "  Qt6:          ${Qt6_VERSION}")
message(STATUS "  libssh:       found")
message(STATUS "  QTermWidget6: found")
message(STATUS "  Qt6Keychain:  found")
message(STATUS "  System deps:  ${EASY_SSH_USE_SYSTEM_PACKAGES}")
message(STATUS "  Fetch Qt:     ${EASY_SSH_FETCH_QT}")
message(STATUS "  Bundle RT:    ${EASY_SSH_BUNDLE_RUNTIME}")
