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

- CMake 3.21+
- A C++20 compiler
- Qt 6.6+ (Widgets)
- [libssh](https://www.libssh.org/) (>= 0.11, required for ProxyJump)
- [QTermWidget](https://github.com/lxqt/qtermwidget)
- [QtKeychain](https://github.com/frankosterfeld/qtkeychain)

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

```bash
cmake --preset debug
cmake --build --preset debug
./build/easy-ssh
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

Release builds:

```bash
cmake --preset release
cmake --build --preset release
./build-release/easy-ssh
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

CI builds self-contained archives via [`.github/scripts/package.sh`](.github/scripts/package.sh):

- **Linux:** `.AppImage` (primary) and `.tar.gz` fallback tree
- **macOS:** `.dmg`
- **Windows:** single-file portable `.exe` via 7-Zip SFX (fallback: `.zip` if SFX module is unavailable)

Linux and Windows formats bundle Qt and third-party libraries (`libssh`, QTermWidget, QtKeychain).

#### Windows

CI builds on `windows-2022` (MSVC + Qt 6.8 via aqt, libssh via vcpkg, patched QTermWidget).
Locally:

```bash
export PREFIX="$PWD/.deps/prefix"
export BUILD_ROOT="$PWD/.deps/src"
export GITHUB_WORKSPACE="$PWD"
export BUILD_QTKEYCHAIN=1
# After Qt + libssh are available on CMAKE_PREFIX_PATH / CMAKE_TOOLCHAIN_FILE:
.github/scripts/build-qtermwidget.sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PREFIX"
cmake --build build-release
cmake --install build-release --prefix install
# Optional installer (requires NSIS):
cpack -G NSIS --config build-release/CPackConfig.cmake
```

The CMake project embeds `resources/windows/easy-ssh.ico` / `.rc` and configures CPack NSIS (Start Menu shortcut, uninstall).

Portable single-file builds:

- Packaging script prefers a 7-Zip installer SFX module (`7zS.sfx` / `7zSD.sfx`) and emits `easy-ssh-<target>-portable.exe`.
- The SFX executable extracts bundled runtime files to a temp directory and launches `easy-ssh.exe` automatically.
- If your build machine keeps SFX modules in a non-standard path, set `SFX_MODULE_PATH` before running `.github/scripts/package.sh`.

## Contributing

Issues and pull requests are welcome at
[github.com/magiskboy/easy-ssh](https://github.com/magiskboy/easy-ssh).

## License

See the repository for license information.
