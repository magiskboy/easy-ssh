# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# SwiftUI + SwiftTerm + ObjC++ bridge over easy-ssh-core (Apple only).
# Swift is built via `swift build` (Unix Makefiles generator cannot enable_language(Swift)).
# Bridge .mm files compile as CXX with -x objective-c++ (Unix Makefiles lacks OBJCXX rules).

find_program(EASY_SSH_SWIFT NAMES swift REQUIRED)

include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Declare(
    SwiftTerm
    GIT_REPOSITORY https://github.com/migueldeicaza/SwiftTerm.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(SwiftTerm)
if(NOT swiftterm_POPULATED)
    FetchContent_Populate(SwiftTerm)
endif()

set(EASY_SSH_BRIDGE_SOURCES
    bridge/EasySshRuntime.mm
    bridge/ESSSessionController.mm
    bridge/ESSConnectionStore.mm
    bridge/ESSSecretStore.mm
    bridge/ESSAppSettings.mm
    bridge/ESSWorkspaceStore.mm
    bridge/ESSSshConfigParser.mm
)
set_source_files_properties(${EASY_SSH_BRIDGE_SOURCES} PROPERTIES
    LANGUAGE CXX
    COMPILE_FLAGS "-x objective-c++ -fobjc-arc"
)

add_library(easy-ssh-bridge SHARED
    ${EASY_SSH_BRIDGE_SOURCES}
    bridge/EasySshRuntime.h
    bridge/ESSSessionController.h
    bridge/ESSConnectionStore.h
    bridge/ESSConnectionMapping.h
    bridge/ESSSecretStore.h
    bridge/ESSAppSettings.h
    bridge/ESSWorkspaceStore.h
    bridge/ESSSshConfigParser.h
)
target_include_directories(easy-ssh-bridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/bridge
)
target_link_libraries(easy-ssh-bridge
    PUBLIC
        easy-ssh-core
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        "-framework Foundation"
        "-framework AppKit"
)
target_compile_definitions(easy-ssh-bridge
    PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
)
set_target_properties(easy-ssh-bridge PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME EasySshBridge
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    BUILD_RPATH "@loader_path;${CMAKE_BINARY_DIR}/bin"
    INSTALL_RPATH "@loader_path;@loader_path/../Frameworks"
)

set(EASY_SSH_NATIVE_APP_DIR "${CMAKE_BINARY_DIR}/bin/easy-ssh-native.app")
set(EASY_SSH_NATIVE_MACOS_DIR "${EASY_SSH_NATIVE_APP_DIR}/Contents/MacOS")
set(EASY_SSH_NATIVE_EXE "${EASY_SSH_NATIVE_MACOS_DIR}/easy-ssh-native")
set(EASY_SSH_NATIVE_FW_DIR "${EASY_SSH_NATIVE_APP_DIR}/Contents/Frameworks")
set(EASY_SSH_NATIVE_SWIFT_BUILD_DIR "${CMAKE_BINARY_DIR}/macos-swift")
set(EASY_SSH_NATIVE_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/swift-package")

set(EASY_SSH_NATIVE_SWIFT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/app/EasySshApp.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Support/QtRuntimeBootstrap.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Support/WindowFrameStore.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/AppModel.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/StatusBannerModel.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/SessionViewModel.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/ConnectionDraft.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/ContentView.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/ConnectSheet.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/SessionContainerView.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/HostKeySheet.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/PlaceholderFeatureView.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/AppStatusBar.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/EmptyStateView.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/AboutView.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Terminal/TerminalRepresentable.swift"
)

set(swiftterm_SOURCE_DIR "${swiftterm_SOURCE_DIR}")
get_filename_component(swiftterm_PACKAGE_IDENTITY "${swiftterm_SOURCE_DIR}" NAME)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Package.swift.in"
    "${EASY_SSH_NATIVE_PACKAGE_DIR}/Package.swift"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    @ONLY
)

set(_bridge_lib_dir "${CMAKE_BINARY_DIR}/bin")

add_custom_command(
    OUTPUT "${EASY_SSH_NATIVE_EXE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EASY_SSH_NATIVE_MACOS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EASY_SSH_NATIVE_FW_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EASY_SSH_NATIVE_APP_DIR}/Contents"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        "${EASY_SSH_NATIVE_APP_DIR}/Contents/Info.plist"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/EasySshApp.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/EasySshApp.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Support/QtRuntimeBootstrap.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/QtRuntimeBootstrap.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Support/WindowFrameStore.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/WindowFrameStore.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/AppModel.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/AppModel.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/StatusBannerModel.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/StatusBannerModel.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/SessionViewModel.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/SessionViewModel.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Models/ConnectionDraft.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ConnectionDraft.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/ContentView.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ContentView.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/ConnectSheet.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ConnectSheet.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/SessionContainerView.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/SessionContainerView.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/HostKeySheet.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/HostKeySheet.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/PlaceholderFeatureView.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/PlaceholderFeatureView.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/AppStatusBar.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/AppStatusBar.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/EmptyStateView.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/EmptyStateView.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Views/AboutView.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/AboutView.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app/Terminal/TerminalRepresentable.swift"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/TerminalRepresentable.swift"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/EasySsh-Bridging-Header.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/EasySsh-Bridging-Header.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/EasySshRuntime.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/EasySshRuntime.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSSessionController.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ESSSessionController.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSConnectionStore.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ESSConnectionStore.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSSecretStore.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ESSSecretStore.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSAppSettings.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ESSAppSettings.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSWorkspaceStore.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ESSWorkspaceStore.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSSshConfigParser.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/ESSSshConfigParser.h"
    COMMAND ${EASY_SSH_SWIFT} build
        --package-path "${EASY_SSH_NATIVE_PACKAGE_DIR}"
        --configuration debug
        --scratch-path "${EASY_SSH_NATIVE_SWIFT_BUILD_DIR}"
        -Xswiftc -import-objc-header
        -Xswiftc "${EASY_SSH_NATIVE_PACKAGE_DIR}/Sources/EasySshNative/EasySsh-Bridging-Header.h"
        -Xlinker -L${_bridge_lib_dir}
        -Xlinker -lEasySshBridge
        -Xlinker -rpath
        -Xlinker @executable_path/../Frameworks
        -Xlinker -rpath
        -Xlinker ${_bridge_lib_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EASY_SSH_NATIVE_SWIFT_BUILD_DIR}/debug/easy-ssh-native"
        "${EASY_SSH_NATIVE_EXE}"
    DEPENDS
        easy-ssh-bridge
        ${EASY_SSH_NATIVE_SWIFT_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/EasySsh-Bridging-Header.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/EasySshRuntime.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSSessionController.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSConnectionStore.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSSecretStore.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSAppSettings.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSWorkspaceStore.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/ESSSshConfigParser.h"
        "${EASY_SSH_NATIVE_PACKAGE_DIR}/Package.swift"
    COMMENT "Building easy-ssh-native (SwiftUI + SwiftTerm)"
    VERBATIM
)

add_custom_target(easy-ssh-native ALL
    DEPENDS "${EASY_SSH_NATIVE_EXE}"
)

add_custom_command(TARGET easy-ssh-native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EASY_SSH_NATIVE_FW_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:easy-ssh-bridge>"
        "${EASY_SSH_NATIVE_FW_DIR}/"
    VERBATIM
)

if(TARGET ssh)
    add_custom_command(TARGET easy-ssh-native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ssh>"
            "${EASY_SSH_NATIVE_FW_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE_NAME:ssh>"
            "${EASY_SSH_NATIVE_FW_DIR}/libssh.4.dylib"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE_NAME:ssh>"
            "${EASY_SSH_NATIVE_FW_DIR}/libssh.dylib"
        VERBATIM
    )
endif()

if(DEFINED EASY_SSH_QT_PREFIX)
    set(_qt_lib "${EASY_SSH_QT_PREFIX}/lib")
    foreach(_fwname QtCore QtGui QtNetwork QtDBus)
        if(EXISTS "${_qt_lib}/${_fwname}.framework")
            add_custom_command(TARGET easy-ssh-native POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt_lib}/${_fwname}.framework"
                    "${EASY_SSH_NATIVE_FW_DIR}/${_fwname}.framework"
                VERBATIM
            )
        endif()
    endforeach()
endif()

add_custom_command(TARGET easy-ssh-native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/bin/libqt6keychain.1.dylib"
        "${EASY_SSH_NATIVE_FW_DIR}/libqt6keychain.1.dylib"
    # Sign nested frameworks first; --deep on the .app fails with Qt framework layout.
    COMMAND /bin/bash -c "for f in '${EASY_SSH_NATIVE_FW_DIR}'/*.framework '${EASY_SSH_NATIVE_FW_DIR}'/*.dylib; do [ -e \"$$f\" ] && codesign --force --sign - \"$$f\" || true; done"
    COMMAND codesign --force --sign - "${EASY_SSH_NATIVE_EXE}"
    COMMAND codesign --force --sign - "${EASY_SSH_NATIVE_APP_DIR}"
    COMMENT "Bundle deps + codesign easy-ssh-native.app"
    VERBATIM
)
