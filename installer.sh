#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Install the latest Easy SSH release for the current OS / architecture.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh | bash
#   bash <(curl -fsSL https://raw.githubusercontent.com/magiskboy/easy-ssh/main/installer.sh)
#
# Optional environment variables:
#   EASY_SSH_VERSION   Release tag to install (default: latest), e.g. v1.0.0
#   EASY_SSH_PREFIX    Linux install prefix (default: ~/.local)
#   EASY_SSH_REPO      GitHub owner/repo (default: magiskboy/easy-ssh)
set -euo pipefail

REPO="${EASY_SSH_REPO:-magiskboy/easy-ssh}"
PREFIX="${EASY_SSH_PREFIX:-${HOME}/.local}"
VERSION="${EASY_SSH_VERSION:-}"
GITHUB_API="https://api.github.com"
GITHUB_DL="https://github.com"

_CLEANUP_FILES=()
_CLEANUP_DIRS=()
_DMG_MOUNT=""

cleanup() {
  local f d
  for f in "${_CLEANUP_FILES[@]:-}"; do
    rm -f "$f"
  done
  for d in "${_CLEANUP_DIRS[@]:-}"; do
    rm -rf "$d"
  done
  if [[ -n "${_DMG_MOUNT}" ]]; then
    hdiutil detach "${_DMG_MOUNT}" -quiet >/dev/null 2>&1 || true
    rmdir "${_DMG_MOUNT}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

log()  { printf 'easy-ssh: %s\n' "$*" >&2; }
die()  { printf 'easy-ssh: error: %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || die "'$1' is required"; }

detect_os() {
  case "$(uname -s)" in
    Linux)  echo linux ;;
    Darwin) echo macos ;;
    *) die "unsupported OS: $(uname -s) (Linux and macOS only)" ;;
  esac
}

detect_arch() {
  case "$(uname -m)" in
    x86_64|amd64)           echo amd64 ;;
    aarch64|arm64|armv8*)   echo arm64 ;;
    *) die "unsupported architecture: $(uname -m)" ;;
  esac
}

download() {
  local url="$1" dest="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --proto '=https' --tlsv1.2 -o "$dest" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O "$dest" "$url"
  else
    die "curl or wget is required"
  fi
}

http_get() {
  local url="$1"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --proto '=https' --tlsv1.2 "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O - "$url"
  else
    die "curl or wget is required"
  fi
}

resolve_version() {
  if [[ -n "$VERSION" ]]; then
    # Accept "1.0.0" or "v1.0.0".
    [[ "$VERSION" == v* ]] || VERSION="v${VERSION}"
    echo "$VERSION"
    return
  fi
  local json tag
  json="$(http_get "${GITHUB_API}/repos/${REPO}/releases/latest")" || \
    die "failed to query latest release (check network / GitHub API)"
  tag="$(printf '%s' "$json" | sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)"
  [[ -n "$tag" ]] || die "could not parse latest release tag"
  echo "$tag"
}

asset_name() {
  local os="$1" arch="$2"
  case "$os" in
    linux) echo "easy-ssh-linux-${arch}.tar.gz" ;;
    macos) echo "easy-ssh-macos-${arch}.dmg" ;;
  esac
}

path_has_dir() {
  local dir="$1"
  case ":${PATH}:" in
    *":${dir}:"*) return 0 ;;
    *) return 1 ;;
  esac
}

install_linux() {
  local archive="$1" version="$2"
  need tar

  local dest="${PREFIX}/opt/easy-ssh"
  local bindir="${PREFIX}/bin"
  local tmp
  tmp="$(mktemp -d "${TMPDIR:-/tmp}/easy-ssh-install.XXXXXX")"
  _CLEANUP_DIRS+=("$tmp")

  log "Extracting…"
  tar -xzf "$archive" -C "$tmp"

  local staged="${tmp}/easy-ssh"
  [[ -x "${staged}/bin/easy-ssh" ]] || die "archive missing bin/easy-ssh"

  log "Installing to ${dest}"
  mkdir -p "$(dirname "$dest")" "$bindir"
  rm -rf "$dest"
  mv "$staged" "$dest"

  ln -sfn "${dest}/bin/easy-ssh" "${bindir}/easy-ssh"
  chmod +x "${dest}/bin/easy-ssh"

  # Desktop entry + icons for app menus (XDG user dirs).
  if [[ -d "${dest}/share" ]]; then
    mkdir -p "${PREFIX}/share"
    if command -v rsync >/dev/null 2>&1; then
      rsync -a "${dest}/share/" "${PREFIX}/share/"
    else
      cp -a "${dest}/share/." "${PREFIX}/share/"
    fi
    local desktop="${PREFIX}/share/applications/io.github.magiskboy.easy-ssh.desktop"
    if [[ -f "$desktop" ]]; then
      # Point Exec at the installed binary so PATH is not required.
      sed -i "s|^Exec=.*|Exec=${bindir}/easy-ssh|" "$desktop"
    fi
  fi

  printf '%s\n' "$version" >"${dest}/VERSION"

  log "Installed Easy SSH ${version}"
  log "Binary: ${bindir}/easy-ssh"
  if ! path_has_dir "$bindir"; then
    log "Add to your shell profile:"
    log "  export PATH=\"${bindir}:\$PATH\""
  fi
  log "Run: easy-ssh"
}

install_macos() {
  local dmg="$1" version="$2"
  need hdiutil

  local mount
  mount="$(mktemp -d "${TMPDIR:-/tmp}/easy-ssh-dmg.XXXXXX")"
  _DMG_MOUNT="$mount"

  log "Mounting DMG…"
  hdiutil attach "$dmg" -mountpoint "$mount" -nobrowse -quiet

  local app
  app="$(find "$mount" -maxdepth 2 -name '*.app' -type d | head -1 || true)"
  [[ -n "$app" && -d "$app" ]] || die "no .app bundle found in DMG"

  local app_name
  app_name="$(basename "$app")"
  # Prefer a friendly name in /Applications when the bundle is easy-ssh.app.
  local target_name="$app_name"
  if [[ "$app_name" == "easy-ssh.app" ]]; then
    target_name="Easy SSH.app"
  fi

  local dest="/Applications/${target_name}"
  log "Installing to ${dest}"
  if [[ -e "$dest" ]]; then
    if [[ -w /Applications ]] || [[ -w "$dest" ]]; then
      rm -rf "$dest"
    else
      need sudo
      sudo rm -rf "$dest"
    fi
  fi
  if [[ -w /Applications ]]; then
    cp -R "$app" "$dest"
  else
    need sudo
    sudo cp -R "$app" "$dest"
  fi

  hdiutil detach "$mount" -quiet || true
  rmdir "$mount" 2>/dev/null || true
  _DMG_MOUNT=""

  log "Installed Easy SSH ${version}"
  log "Open: open -a \"${target_name%.app}\""
}

main() {
  local os arch version asset url tmpfile
  os="$(detect_os)"
  arch="$(detect_arch)"
  version="$(resolve_version)"
  asset="$(asset_name "$os" "$arch")"
  url="${GITHUB_DL}/${REPO}/releases/download/${version}/${asset}"

  log "Detected ${os}/${arch}"
  log "Installing ${version} (${asset})"

  tmpfile="$(mktemp "${TMPDIR:-/tmp}/easy-ssh-XXXXXX")"
  _CLEANUP_FILES+=("$tmpfile")

  log "Downloading ${url}"
  if ! download "$url" "$tmpfile"; then
    die "download failed — is ${asset} published for ${version}?"
  fi

  case "$os" in
    linux) install_linux "$tmpfile" "$version" ;;
    macos) install_macos "$tmpfile" "$version" ;;
  esac
}

main "$@"
