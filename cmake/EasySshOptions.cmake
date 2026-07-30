include_guard(GLOBAL)

option(EASY_SSH_BUNDLE_RUNTIME
    "Bundle non-system runtime libraries into the install tree (for CPack fat packages)"
    OFF)

# Optional CI artifact suffix, e.g. linux-amd64 (passed from GitHub Actions matrix).
set(EASY_SSH_PACKAGE_SUFFIX "" CACHE STRING
    "Suffix for CPack output file names (e.g. linux-amd64)")
