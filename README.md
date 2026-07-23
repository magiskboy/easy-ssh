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

> **Note:** Linux is the primary development and test target today. Other platforms depend on available Qt, libssh, and QTermWidget packages.

## Development

### Prerequisites

- CMake 3.21+
- A C++20 compiler
- Qt 6.6+ (Widgets)
- [libssh](https://www.libssh.org/)
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

Release builds:

```bash
cmake --preset release
cmake --build --preset release
./build-release/easy-ssh
```

## Contributing

Issues and pull requests are welcome at
[github.com/magiskboy/easy-ssh](https://github.com/magiskboy/easy-ssh).

## License

See the repository for license information.
