include_guard(GLOBAL)

# Optional CI artifact suffix, e.g. linux-amd64 (passed from GitHub Actions matrix).
set(EASY_SSH_PACKAGE_SUFFIX "" CACHE STRING
    "Suffix for CPack output file names (e.g. linux-amd64)")

# Qt Widgets desktop shell (easy-ssh). Default ON everywhere.
option(EASY_SSH_BUILD_QT_WIDGETS "Build Qt Widgets desktop app (easy-ssh)" ON)

# SwiftUI + SwiftTerm shell on macOS (easy-ssh-native). Independent of Qt Widgets.
if(APPLE)
    option(EASY_SSH_NATIVE_MACOS "Build SwiftUI+SwiftTerm macOS app (easy-ssh-native)" ON)
else()
    set(EASY_SSH_NATIVE_MACOS OFF CACHE BOOL
        "Build SwiftUI+SwiftTerm macOS app (easy-ssh-native)" FORCE)
endif()

if(NOT EASY_SSH_BUILD_QT_WIDGETS AND NOT EASY_SSH_NATIVE_MACOS)
    message(FATAL_ERROR
        "Easy SSH: enable at least one UI — EASY_SSH_BUILD_QT_WIDGETS and/or "
        "EASY_SSH_NATIVE_MACOS (Apple only).")
endif()

if(EASY_SSH_NATIVE_MACOS AND NOT APPLE)
    message(FATAL_ERROR "Easy SSH: EASY_SSH_NATIVE_MACOS requires Apple platforms.")
endif()
