# Easy SSH

A lightweight, native SSH client for everyday remote work.

Easy SSH focuses on what you need most: a solid interactive terminal, remote file management over SFTP, and simple SSH tunnels — without the weight of a full remote-desktop suite.

**Repository:** [github.com/magiskboy/easy-ssh](https://github.com/magiskboy/easy-ssh)

## Features

### Connections

- Create, edit, duplicate, and delete saved connections
- Search by name, host, or username
- Password or OpenSSH private-key authentication
- Passwords and passphrases stored in the system keychain
- Optional startup directory per connection

### Sessions

- Open multiple independent sessions from one connection
- Tabbed sessions with reorder and close
- Manual reconnect when a session drops
- Disconnect one session without affecting others

### Terminal

- Full interactive shell (bash, zsh, fish, …)
- ANSI colors and UTF-8
- Copy / paste, search scrollback, clear screen
- Save session log and PNG screenshots
- Terminal resize follows the window

### Remote files (SFTP)

- Browse the remote filesystem in a tree view
- Upload and download files and folders
- Create folders, rename, and delete
- Progress and cancel for transfers
- Open With: edit remotely via your local apps, with auto-upload on save

### SSH tunnels

- Local and remote port forwarding
- Per-connection tunnel list with enable / disable
- Status visible while the session is active

### Settings

- Terminal font and size
- Default download directory
- Auto-reconnect preference
- Confirm before delete
- Keyboard shortcuts for common actions

## Platforms

Built with Qt 6 for a native look on **Linux**, **Windows**, and **macOS**.

> **Note:** Linux is the primary development and test target today. Windows builds
> use a patched QTermWidget with direct VT feed (no local PTY / ConPTY). See
> [`third_party/qtermwidget-patches/`](third_party/qtermwidget-patches/).

## Install (Linux / macOS)

Download and install the latest GitHub release for your OS and CPU:

```bash
curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh | bash
```

The script detects Linux/macOS and amd64/arm64, then installs:

- **Linux:** `easy-ssh-*-*.tar.gz` under `~/.local/opt/easy-ssh`, with a symlink at `~/.local/bin/easy-ssh`
- **macOS:** `easy-ssh-*-*.dmg` into `/Applications`

Optional:

```bash
# Pin a release tag
EASY_SSH_VERSION=v1.0.0 curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh | bash

# Custom Linux prefix (default: ~/.local)
EASY_SSH_PREFIX=/usr/local curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh | bash
```

## Development

### Prerequisites

- CMake 3.28+
- A C++20 compiler
- Qt 6.6+ (Widgets)
- [libssh](https://www.libssh.org/) (>= 0.11, required for ProxyJump)
- [QTermWidget](https://github.com/lxqt/qtermwidget)
- [QtKeychain](https://github.com/frankosterfeld/qtkeychain)

Version pins for CI and FetchContent builds live in
[`cmake/EasySshVersions.cmake`](cmake/EasySshVersions.cmake).

#### Fedora

```bash
sudo dnf install -y --setopt=install_weak_deps=False \
  cmake \
  gcc-c++ \
  pkgconf-pkg-config \
  qt6-qtbase-devel \
  libssh-devel \
  qtermwidget-devel \
  qtkeychain-qt6-devel
```

### Build

Two presets:

| Preset | Purpose |
|--------|---------|
| `debug` | Local development (system Qt/libs; no runtime bundling) |
| `release` | Fat packages / CI (aqt Qt + FetchContent deps + bundle runtime) |

**Local development:**

```bash
cmake --preset debug
cmake --build --preset debug
./build/bin/easy-ssh
```

On Windows, if libssh / QTermWidget / QtKeychain are not installed, the `debug`
preset still allows FetchContent for those libraries. Install Qt 6 locally (or set
`CMAKE_PREFIX_PATH`); `EASY_SSH_FETCH_QT` stays off for debug.

**Release / fat package build:**

```bash
cmake --preset release
cmake --build --preset release
./build-release/bin/easy-ssh
```

Requires Python 3 + pip when `EASY_SSH_FETCH_QT=ON` (`aqtinstall` downloads Qt).
First configure may take several minutes while Qt and FetchContent deps download.

Install pip-managed toolchains so local and CI match. Core set (aqtinstall,
clang-format, cmake, ninja, patchelf) is what CI uses:

```bash
python3 -m pip install -r requirements.txt
```

On macOS (Homebrew Python / PEP 668), prefer a venv — CI uses the same approach:

```bash
python3 -m venv .deps/venv
source .deps/venv/bin/activate
python -m pip install -r requirements.txt
```

Or run `.github/scripts/install-pip-toolchains.sh` and put its printed `bin` directory on `PATH`.

Optional local extras (clang-tidy, clang-include-cleaner, icnsutil):

```bash
python3 -m pip install -r requirements-dev.txt
```

Ensure the install bin directory is on `PATH` (venv `bin/` / `Scripts`, or `~/.local/bin` with
`--user`).

**Not available via pip** (kept as OS / GitHub Actions tooling):

| Need | How CI provides it | Why not pip |
|------|--------------------|-------------|
| Linux Qt runtime (xcb, OpenGL, …) | [`ci/apt-linux.txt`](ci/apt-linux.txt) via apt | System `.so` libs; [Qt Linux requirements](https://doc.qt.io/qt-6/linux-requirements.html) |
| MSVC C++ compiler | `ilammy/msvc-dev-cmd` on `windows-2022` | No MSVC wheel on PyPI; `aqtinstall` can install MinGW/vcredist only |
| appimagetool | [`.github/scripts/fetch-appimagetool.sh`](.github/scripts/fetch-appimagetool.sh) (pinned release) | Official binary is an AppImage, not a PyPI package |

```bash
# Linux system packages (example):
sudo apt-get install -y --no-install-recommends $(grep -vE '^\s*(#|$)' ci/apt-linux.txt)
```

Enable local git hooks (clang-format pre-commit, same check as CI):

```bash
.github/scripts/install-git-hooks.sh
```

Format all C++ sources:

```bash
.github/scripts/run-clang-format.sh
.github/scripts/run-clang-format-check.sh
```

### Icons

Application icons are generated from [`icon.png`](icon.png):

```bash
./scripts/generate-icons.sh
```

Requires ImageMagick. macOS `.icns` needs `iconutil` (macOS) or the Python package `icnsutil`. Re-run the script after changing `icon.png` and commit the updated files under `resources/`.

### Packaging / install

Install desktop metadata, icons, and the binary into a prefix:

```bash
cmake --install build-release --prefix /usr/local
```

On Linux this installs:

- `bin/easy-ssh`
- `share/applications/io.github.magiskboy.easy-ssh.desktop`
- `share/metainfo/io.github.magiskboy.easy-ssh.metainfo.xml`
- hicolor icons under `share/icons/hicolor/...`

**Self-contained packages (CPack):**

```bash
cmake --preset release
cmake --build --preset release
cmake --install build-release --prefix staging
cd build-release
cpack -C Release -G "DEB;RPM;TGZ"   # Linux
# AppImage: use .github/scripts/build-appimage.sh with appimagetool (not CPack)
# cpack -C Release -G DragNDrop              # macOS
# cpack -C Release -G NSIS                   # Windows (requires NSIS)
```

CI produces fat packages (bundled Qt, libssh, QTermWidget, QtKeychain):

- **Linux:** `.deb`, `.rpm`, `.AppImage`, `.tar.gz`
- **macOS:** `.dmg`
- **Windows:** NSIS `.exe` installer

#### Windows (local)

Use the `release` preset with MSVC and NSIS installed:

```bash
cmake --preset release
cmake --build --preset release
cmake --install build-release --prefix staging
cpack -G NSIS --config build-release/CPackConfig.cmake
```

The CMake project embeds `resources/windows/easy-ssh.ico` / `.rc` and configures
CPack NSIS (Start Menu shortcut, uninstall).

## Contributing

Issues and pull requests are welcome at
[github.com/magiskboy/easy-ssh](https://github.com/magiskboy/easy-ssh).

## License

See the repository for license information.
