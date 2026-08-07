include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Portable fat packages / CPack AppImage expect libs under lib/ (not lib64).
# CPack AppImage hardcodes RPATH to $ORIGIN/../lib.
if(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Object code libraries (lib)" FORCE)
endif()

# Put shared runtime next to easy-ssh so windeployqt / dyld can resolve
# FetchContent DLLs (qt6keychain, ssh, qtermwidget) from the build tree.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
if(WIN32)
    # Windows shared libs are RUNTIME artifacts (.dll), not LIBRARY (.lib).
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# Portable install layout: look for bundled libs next to the binary.
if(NOT APPLE AND NOT WIN32)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
elseif(APPLE)
    # FetchContent shared libs land in ${CMAKE_BINARY_DIR}/lib; resolve via @rpath
    # (QTermWidget otherwise hardcodes INSTALL_NAME_DIR=/usr/local/lib).
    list(APPEND CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/lib")
endif()
