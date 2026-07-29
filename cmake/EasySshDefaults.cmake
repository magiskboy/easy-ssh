include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Portable install layout: look for bundled libs next to the binary.
if(NOT APPLE AND NOT WIN32)
    set(CMAKE_INSTALL_RPATH
        "$ORIGIN/../lib:$ORIGIN/../lib64:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/EasySshQt.cmake)

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Network
)

qt_standard_project_setup()
