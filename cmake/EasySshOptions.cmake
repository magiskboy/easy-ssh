include_guard(GLOBAL)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_easy_ssh_it_default ON)
else()
    set(_easy_ssh_it_default OFF)
endif()

option(EASY_SSH_USE_SYSTEM_PACKAGES
    "Use find_package for libssh/qtermwidget/qtkeychain instead of FetchContent"
    ON)
option(EASY_SSH_FETCH_QT
    "Download Qt via aqtinstall when find_package(Qt6) fails or Qt is missing"
    OFF)
option(EASY_SSH_BUNDLE_RUNTIME
    "Bundle non-system runtime libraries into the install tree (for CPack fat packages)"
    OFF)
option(EASY_SSH_INTEGRATION_TESTS
    "Build local headless integration tests (Linux only)"
    ${_easy_ssh_it_default})
unset(_easy_ssh_it_default)

# Optional CI artifact suffix, e.g. linux-amd64 (passed from GitHub Actions matrix).
set(EASY_SSH_PACKAGE_SUFFIX "" CACHE STRING
    "Suffix for CPack output file names (e.g. linux-amd64)")
