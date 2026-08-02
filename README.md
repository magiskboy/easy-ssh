# Easy SSH

[![CI](https://github.com/magiskboy/easy-ssh/actions/workflows/ci.yml/badge.svg)](https://github.com/magiskboy/easy-ssh/actions/workflows/ci.yml)
[![Status](https://img.shields.io/badge/status-under%20development-yellow)](https://github.com/magiskboy/easy-ssh)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![REUSE status](https://api.reuse.software/badge/github.com/magiskboy/easy-ssh)](https://api.reuse.software/info/github.com/magiskboy/easy-ssh)

> **Under development** — no stable release yet. APIs, packaging, and features may change without notice. Prefer building from source (see below) until the first tagged release.

A lightweight, native SSH client that puts the full SSH protocol to work — shell, file transfer, and every form of forwarding — in one desktop app.

Built with **Qt 6** and **libssh** for a native look on Linux, Windows, and macOS.

## Features

Easy SSH is designed around the SSH protocol itself: each capability maps to a first-class part of the protocol, not a bolt-on workaround.

**Shell**

- Interactive shell (`ssh user@host`) — multi-tab sessions, ANSI / UTF-8, search, logs, screenshots
- Execute a single remote command and exit *(planned)*

**File transfer**

- **SFTP** — browse, upload / download, mkdir, rename, delete; Open With + auto-upload on save
- **SFTP resume** — interrupted transfers continue from `.filepart` with SHA-256 integrity checks
- **SCP** — fallback when SFTP is unavailable (no resume)

**Forwarding & tunnels**

- **Local** and **remote** (reverse) TCP port forwarding
- **Dynamic** SOCKS5 proxy (`ssh -D`) *(planned)*
- **StreamLocal** — Unix domain socket forwarding *(planned)*
- **X11** forwarding — remote GUI apps over SSH *(planned)*
- **Agent** forwarding *(planned)*

**Connections**

- Saved connections with search; password or OpenSSH private-key auth
- Secrets in the system keychain; optional startup directory
- Per-connection tunnel list with live status

## Platforms


| OS          | Status                                                                                                                                                                          |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Linux**   | Primary development and test target                                                                                                                                             |
| **macOS**   | Supported (DMG packages)                                                                                                                                                        |
| **Windows** | Supported (NSIS installer); uses a patched QTermWidget with direct VT feed (no local PTY / ConPTY) — see [`third_party/qtermwidget-patches/`](third_party/qtermwidget-patches/) |


## Install

> Installer and GitHub Release packages will be available after the first tagged release. Until then, build from source.

### One-line installer (Linux / macOS) — after release

```bash
curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh | bash
```

The script detects OS and CPU (amd64 / arm64), then installs:

- **Linux:** `easy-ssh-*-*.tar.gz` under `~/.local/opt/easy-ssh`, with a symlink at `~/.local/bin/easy-ssh`
- **macOS:** `easy-ssh-*-*.dmg` into `/Applications`

Optional environment variables:

```bash
# Pin a release tag (once releases exist)
EASY_SSH_VERSION=v0.1.0 curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh | bash

# Custom Linux prefix (default: ~/.local)
EASY_SSH_PREFIX=/usr/local curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh | bash
```

### Packages from GitHub Releases — after release

Download a build for your platform from the [Releases](https://github.com/magiskboy/easy-ssh/releases) page once available:


| Platform | Artifacts                              |
| -------- | -------------------------------------- |
| Linux    | `.deb`, `.rpm`, `.AppImage`, `.tar.gz` |
| macOS    | `.dmg`                                 |
| Windows  | NSIS `.exe` installer                  |


Packages are self-contained (bundled Qt, libssh, QTermWidget, QtKeychain).

## Build from source

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for prerequisites, CMake presets, packaging, and coding guidelines.

Quick start (Linux / macOS):

```bash
git clone https://github.com/magiskboy/easy-ssh.git
cd easy-ssh

.github/scripts/install-pip-toolchains.sh
export PATH="$(pwd)/.deps/venv/bin:$PATH"

cmake --preset debug
cmake --build --preset debug
./build/bin/easy-ssh
```

## Contributing

Contributions are welcome — bug reports, feature ideas, documentation, and pull requests.

Please read **[CONTRIBUTING.md](CONTRIBUTING.md)** before opening an issue or PR.

## License

Easy SSH is licensed under the [GNU General Public License v3.0](LICENSE).

Source files carry [SPDX](https://spdx.dev/) headers; the tree aims to stay
[REUSE](https://reuse.software/)-compliant (`REUSE.toml`, `LICENSES/`).

Third-party components are listed in [NOTICE](NOTICE) and remain under their own licenses.