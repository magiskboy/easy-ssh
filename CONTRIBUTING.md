# Contributing to Easy SSH

Thanks for your interest in contributing. This guide covers how to report bugs, propose features, set up a development environment, and submit pull requests.

## Ways to contribute

- **Bug reports** — help us reproduce and fix issues
- **Feature requests** — describe use cases clearly
- **Code** — bug fixes, features, refactoring, tests
- **Documentation** — README, this guide, comments, packaging notes
- **Packaging / CI** — improve builds, installers, and automation

All discussion on GitHub should be in **English** so the widest audience can participate.

## Reporting bugs

Before opening an issue:

1. Search [existing issues](https://github.com/magiskboy/easy-ssh/issues) for duplicates.
2. Confirm the problem still happens on the latest release (or `main`).
3. Note your OS, architecture, and how you installed Easy SSH (installer script, package, or built from source).

A useful bug report includes:

- A clear title and a short summary
- Steps to reproduce
- Expected vs actual behavior
- Easy SSH version (About dialog) and OS version
- Screenshots or logs when relevant

## Feature requests

Open an issue describing:

- The problem you are trying to solve
- How you would use the feature
- Whether similar tools already offer it (and what you like / dislike about those approaches)

## Development setup

### Prerequisites

| Requirement | Notes |
|-------------|--------|
| CMake **4.2+** | Prefer the pip pin in `requirements/requirements.txt` |
| C++20 compiler | GCC, Clang, or MSVC |
| Python 3 + pip | Used by `aqtinstall` to fetch Qt into `.deps/qt` |
| OS packages | Toolchain / Qt runtime glue only (compiler, xcb, OpenGL, OpenSSL) — **not** distro Qt, libssh, QTermWidget, or QtKeychain |

Pinned versions for Qt and FetchContent libraries live in [`cmake/EasySshVersions.cmake`](cmake/EasySshVersions.cmake).

#### Fedora (toolchain + OS runtime only)

```bash
sudo dnf install -y --setopt=install_weak_deps=False \
  gcc-c++ \
  pkgconf-pkg-config \
  openssl-devel \
  zlib-devel
```

#### Debian / Ubuntu (CI-equivalent)

```bash
sudo apt-get install -y --no-install-recommends \
  $(grep -vE '^\s*(#|$)' .github/scripts/apt-linux.txt)
```

### Toolchains (pip)

Install the same tooling CI uses:

```bash
.github/scripts/install-pip-toolchains.sh
export PATH="$(pwd)/.deps/venv/bin:$PATH"
```

Or manually:

```bash
python3 -m venv .deps/venv
source .deps/venv/bin/activate   # Windows: .deps\venv\Scripts\activate
python -m pip install -r requirements/requirements.txt
```

Optional extras (clang-tidy, clang-include-cleaner, icnsutil, reuse):

```bash
python -m pip install -r requirements/requirements-dev.txt
```

**Not available via pip** (kept as OS / GitHub Actions tooling):

| Need | How CI provides it | Why not pip |
|------|--------------------|-------------|
| Linux Qt runtime (xcb, OpenGL, …) | [`.github/scripts/apt-linux.txt`](.github/scripts/apt-linux.txt) | System `.so` libs |
| MSVC C++ compiler | `ilammy/msvc-dev-cmd` on `windows-2022` | No MSVC wheel on PyPI |
| appimagetool | [`.github/scripts/fetch-appimagetool.sh`](.github/scripts/fetch-appimagetool.sh) | Official binary is an AppImage |

### Build

Three CMake presets (all use local aqt Qt + FetchContent deps):

| Preset | Purpose | Binary dir |
|--------|---------|------------|
| `debug` | Local development | `build/` |
| `release` | Packages / CI | `build-release/` |
| `tests` | Unit tests (CI / local) | `build-tests/` |

```bash
# Debug (day-to-day)
cmake --preset debug
cmake --build --preset debug
./build/bin/easy-ssh

# Release
cmake --preset release
cmake --build --preset release
./build-release/bin/easy-ssh
```

The first configure downloads Qt (currently **6.10.3**) into `.deps/qt` and builds FetchContent deps (libssh, QTermWidget, QtKeychain). That can take several minutes.

### Unit tests

Use the project-local Python/CMake toolchain from `.venv` and the `tests`
preset (same flow as the CI `unit tests` job):

```bash
source .venv/bin/activate

cmake --preset tests
cmake --build --preset tests
ctest --preset tests
```

Notes:

- The `tests` preset filters to Easy SSH's own executables (`^tst_`) and
  enables `--output-on-failure`.
- `build-tests/` keeps unit-test artifacts separate from the normal `debug` and
  `release` preset trees.
- To rebuild just one suite, target its executable name, for example
  `cmake --build --preset tests --target tst_ProcessParser`.
- To run one suite only, filter by its test name, for example
  `ctest --preset tests -R '^tst_ProcessParser$'`.

### Editor / LSP (clangd)

Recommended extensions are listed in [`.vscode/extensions.json`](.vscode/extensions.json) (clangd, CMake Tools, EditorConfig). Workspace settings in [`.vscode/settings.json`](.vscode/settings.json) disable the Microsoft C/C++ IntelliSense engine so it does not conflict with clangd.

1. Configure a **debug** build so `build/compile_commands.json` exists (see above). clangd reads that path only (not a root-level copy).
2. [`.clangd`](.clangd) points clangd at `build/`, and skips diagnostics plus background indexing under `build/`, `.deps/`, and `third_party/`.
3. Formatting uses [`.clang-format`](.clang-format); lint checks use [`.clang-tidy`](.clang-tidy).

Indent and newline defaults for other editors are in [`.editorconfig`](.editorconfig).

### Git hooks and formatting

Enable the pre-commit hook (clang-format **auto-fix** on staged C++ + REUSE lint).
CI still runs `clang-format --check` as a gate:

```bash
.github/scripts/install-git-hooks.sh
export PATH="$(pwd)/.deps/venv/bin:$PATH"   # if tools live in the project venv
```

Requires `clang-format` (`requirements/requirements.txt`) and `reuse`
(`requirements/requirements-dev.txt`) on `PATH`.

Format / check C++ sources:

```bash
.github/scripts/run-clang-format.sh                    # apply all under src/
.github/scripts/run-clang-format.sh --check            # check all under src/ (CI)
.github/scripts/run-clang-format.sh --check path/a.cpp # check specific files
```

Style is defined in [`.clang-format`](.clang-format). Pre-commit reformats staged C++ in place (and re-stages); the CI `clang-format` job still fails if the tree is unformatted.

### License headers (SPDX / REUSE)

C and C++ sources use SPDX headers, for example:

```cpp
// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only
```

Bulk annotations for icons, GitHub metadata, and other non-source files live in [`REUSE.toml`](REUSE.toml). License texts are under [`LICENSES/`](LICENSES/).

Install the REUSE tool (`requirements/requirements-dev.txt`), then:

```bash
# Check the whole tree (also runs in CI)
.github/scripts/run-reuse-lint.sh

# Add headers to new C++ files (defaults to all of src/)
.github/scripts/run-reuse-annotate.sh path/to/NewFile.cpp path/to/NewFile.h
```

### Static analysis (clang-tidy)

Install tidy tooling (`requirements/requirements-dev.txt`), configure the debug preset, then:

```bash
.github/scripts/run-clang-tidy.sh

# Optional: apply fixes in place
EASY_SSH_TIDY_FIX=1 .github/scripts/run-clang-tidy.sh

# Use a different build tree (e.g. release)
EASY_SSH_BUILD_DIR=build-release .github/scripts/run-clang-tidy.sh
```

### Icons

Application icons are generated from [`icon.png`](icon.png):

```bash
./scripts/generate-icons.sh
```

Requires ImageMagick. macOS `.icns` needs `iconutil` (macOS) or the Python package `icnsutil`. Re-run after changing `icon.png` and commit the updated files under `resources/`.

## Project layout

```
src/
  app/          # main() entry
  core/         # SSH, SFTP/SCP, tunnels, connections, settings (no UI)
  gui/          # Qt widgets, models, dialogs
cmake/          # Versions, deps, install, packaging
resources/      # Icons, desktop/metainfo, platform assets
third_party/    # Vendored deps (QTermWidget, lxqt-build-tools, …)
.github/        # CI workflows and helper scripts
scripts/        # Maintainer utilities (icons, …)
```

When adding features, prefer keeping protocol / session logic in `src/core/` and UI in `src/gui/`.

## Packaging

Install into a prefix:

```bash
cmake --install build-release --prefix /usr/local
```

On Linux this installs the binary, desktop entry, AppStream metainfo, and hicolor icons.

Self-contained packages via CPack:

```bash
cmake --preset release
cmake --build --preset release
cmake --install build-release --prefix staging
cd build-release
cpack -C Release -G "DEB;RPM;TGZ;AppImage"   # Linux (needs appimagetool + patchelf)
# cpack -C Release -G DragNDrop              # macOS
# cpack -C Release -G NSIS                   # Windows (requires NSIS)
```

### Windows notes

Use the `release` preset with MSVC and NSIS installed. The project embeds `resources/windows/easy-ssh.ico` and generates `easy-ssh.rc` from `easy-ssh.rc.in` (version from `PROJECT_VERSION`), then configures CPack NSIS (Start Menu shortcut, uninstall).

## Pull requests

1. Fork the repo and create a topic branch from `main`.
2. Keep changes focused — one concern per PR when practical.
3. Match existing code style; run clang-format (and clang-tidy when practical).
4. Update docs (README / CONTRIBUTING) if behavior or build steps change.
5. Fill in a clear PR description: **what** changed and **why**.
6. Link related issues (`Fixes #123`) when applicable.
7. For UI changes, include a short screenshot or screen recording.

Draft PRs are welcome if you want early feedback.

## Code review

- Be respectful and assume good intent.
- Prefer concrete suggestions over vague criticism.
- Maintainers may ask for changes; that is normal and not a rejection of the contribution.

## Questions

Open a [GitHub Discussion](https://github.com/magiskboy/easy-ssh/discussions) if available, or an issue tagged as a question. For security-sensitive reports, prefer contacting the maintainer privately rather than filing a public issue.
