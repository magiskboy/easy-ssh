<!--
SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>

SPDX-License-Identifier: GPL-3.0-only
-->

# lxqt-build-tools (vendored for Easy SSH)

Tree is [lxqt/lxqt-build-tools](https://github.com/lxqt/lxqt-build-tools)
tag `2.4.0`, unmodified.

Build-only dependency of vendored QTermWidget. Pulled via CMake
`FetchContent` `SOURCE_DIR` → `third_party/lxqt-build-tools`
(see `cmake/EasySshDependencies.cmake`). Base tag pin:
`EASY_SSH_LXQT_BUILD_TOOLS_GIT_TAG` in `cmake/EasySshVersions.cmake`.

Because FetchContent keeps the top-level `CMAKE_PROJECT_NAME` (`easy-ssh`),
Easy SSH still writes a small `lxqt2-build-tools-config.cmake` shim so
`find_package(lxqt2-build-tools)` resolves the generated CMake modules.

Upstream license: see `BSD-3-Clause` / `README.md`.
