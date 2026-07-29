#!/usr/bin/env bash
# Package a self-contained Easy SSH tree for Linux.
# Usage: package-linux.sh <target-triple> <binary-dir> [out-dir]
set -euo pipefail

TARGET="${1:?usage: package-linux.sh <target-triple> <binary-dir> [out-dir]}"
BINARY_DIR="${2:?usage: package-linux.sh <target> <binary-dir> [out-dir]}"
OUT_DIR="${3:-dist}"

mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
BINARY_DIR="$(cd "$BINARY_DIR" && pwd)"

STAGE="$OUT_DIR/stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"

log() { printf 'package: %s\n' "$*" >&2; }

log "Installing into $STAGE"
cmake --install "$BINARY_DIR" --prefix "$STAGE"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
stage_libdir() {
  local conf=""
  if [[ -f "$STAGE/bin/qt.conf" ]]; then
    conf="$STAGE/bin/qt.conf"
  elif [[ -f "$STAGE/qt.conf" ]]; then
    conf="$STAGE/qt.conf"
  fi
  if [[ -n "$conf" ]]; then
    local libs
    libs="$(sed -n 's/^Libraries *= *//p' "$conf" | head -1 | tr -d '\r')"
    if [[ -n "$libs" ]]; then
      echo "$STAGE/$libs"
      return 0
    fi
  fi
  if [[ -d "$STAGE/lib64" ]]; then
    echo "$STAGE/lib64"
  else
    echo "$STAGE/lib"
  fi
}

qt_plugin_src() {
  if command -v qtpaths6 >/dev/null 2>&1; then
    qtpaths6 --plugin-dir
  elif command -v qtpaths >/dev/null 2>&1; then
    qtpaths --plugin-dir
  elif command -v qmake6 >/dev/null 2>&1; then
    qmake6 -query QT_INSTALL_PLUGINS
  elif command -v qmake >/dev/null 2>&1; then
    qmake -query QT_INSTALL_PLUGINS
  else
    return 1
  fi
}

should_bundle_linux_lib() {
  local path="$1"
  local base
  base="$(basename "$path")"
  case "$base" in
    ld-linux*.so*|libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*)
      return 1 ;;
    libstdc++.so*|libgcc_s.so*|libc++.so*|libc++abi.so*)
      return 1 ;;
    libssh.so*|libqtermwidget*.so*|libqt6keychain.so*|libqtkeychain.so*|libQt6Keychain.so*)
      return 0 ;;
    libQt6*.so*|libqt6*.so*)
      return 0 ;;
  esac
  case "$path" in
    */.deps/prefix/*|*/Cellar/*|*/opt/homebrew/*|*/Qt/*|*/aqtInstall*|*/Qt/6.*)
      return 0 ;;
  esac
  return 1
}

copy_lib_into() {
  local src="$1"
  local libdir="$2"
  mkdir -p "$libdir"
  local base real realbase
  base="$(basename "$src")"
  [[ -e "$libdir/$base" ]] && return 0
  real="$(readlink -f "$src")"
  realbase="$(basename "$real")"
  cp -a "$real" "$libdir/"
  if [[ "$base" != "$realbase" ]]; then
    ln -sfn "$realbase" "$libdir/$base"
  fi
  log "  + $base"
}

bundle_linux_plugins() {
  local libdir="$1"
  local conf="$STAGE/bin/qt.conf"
  local plugins_rel="lib64/qt6/plugins"
  if [[ -f "$conf" ]]; then
    local p
    p="$(sed -n 's/^Plugins *= *//p' "$conf" | head -1 | tr -d '\r')"
    [[ -n "$p" ]] && plugins_rel="$p"
  elif [[ "$(basename "$libdir")" == "lib" ]]; then
    plugins_rel="lib/qt6/plugins"
  fi
  local plugins_dest="$STAGE/$plugins_rel"

  if [[ -d "$plugins_dest/platforms" ]] && compgen -G "$plugins_dest/platforms/libq*" >/dev/null; then
    log "Qt plugins already present under $plugins_rel"
    return 0
  fi

  local src
  src="$(qt_plugin_src)" || {
    log "warning: could not locate Qt plugins directory"
    return 0
  }
  log "Copying Qt plugins from $src → $plugins_rel"
  mkdir -p "$plugins_dest"
  local group
  for group in platforms imageformats iconengines styles tls networkinformation \
               platforminputcontexts platformthemes xcbglintegrations \
               wayland-shell-integration wayland-decoration-client \
               wayland-graphics-integration-client; do
    if [[ -d "$src/$group" ]]; then
      cp -a "$src/$group" "$plugins_dest/"
    fi
  done
}

bundle_linux_deps() {
  local bin
  if [[ -x "$STAGE/bin/easy-ssh" ]]; then
    bin="$STAGE/bin/easy-ssh"
  elif [[ -x "$STAGE/easy-ssh" ]]; then
    bin="$STAGE/easy-ssh"
  else
    echo "error: easy-ssh binary not found under $STAGE after install" >&2
    find "$STAGE" -maxdepth 3 -type f | head -50 >&2 || true
    exit 1
  fi

  local libdir
  libdir="$(stage_libdir)"
  mkdir -p "$libdir"
  log "Bundling runtime deps into $libdir"

  local line path
  while IFS= read -r line; do
    path="$(sed -n 's/.*=> \(.*\) (0x.*/\1/p' <<<"$line")"
    [[ -n "$path" && -e "$path" ]] || continue
    if should_bundle_linux_lib "$path"; then
      copy_lib_into "$path" "$libdir"
    fi
  done < <(ldd "$bin" 2>/dev/null || true)

  local pass
  for pass in 1 2 3 4; do
    local changed=0
    shopt -s nullglob
    for lib in "$libdir"/*.so*; do
      [[ -f "$lib" && ! -L "$lib" ]] || continue
      while IFS= read -r line; do
        path="$(sed -n 's/.*=> \(.*\) (0x.*/\1/p' <<<"$line")"
        [[ -n "$path" && -e "$path" ]] || continue
        if should_bundle_linux_lib "$path"; then
          local base
          base="$(basename "$path")"
          if [[ ! -e "$libdir/$base" ]]; then
            copy_lib_into "$path" "$libdir"
            changed=1
          fi
        fi
      done < <(ldd "$lib" 2>/dev/null || true)
    done
    shopt -u nullglob
    [[ "$changed" -eq 0 ]] && break
  done

  bundle_linux_plugins "$libdir"

  local conf="$STAGE/bin/qt.conf"
  local lib_rel
  lib_rel="$(basename "$libdir")"
  if [[ ! -f "$conf" ]]; then
    cat >"$conf" <<EOF
[Paths]
Prefix = ..
Libraries = ${lib_rel}
Plugins = ${lib_rel}/qt6/plugins
EOF
    log "Wrote $conf"
  fi
}

# ---------------------------------------------------------------------------
# AppImage
# ---------------------------------------------------------------------------
appimage_host_arch() {
  case "${TARGET}" in
    *arm64*|*aarch64*) echo aarch64 ;;
    *amd64*|*x86_64*) echo x86_64 ;;
    *)
      case "$(uname -m)" in
        aarch64|arm64) echo aarch64 ;;
        x86_64|amd64) echo x86_64 ;;
        *)
          echo "error: unsupported arch for AppImage: $(uname -m) (target=$TARGET)" >&2
          return 1
          ;;
      esac
      ;;
  esac
}

fetch_appimagetool() {
  local arch="$1"
  local tools_dir="${APPIMAGE_TOOLS_DIR:-$OUT_DIR/tools}"
  mkdir -p "$tools_dir"
  local tool="$tools_dir/appimagetool-${arch}.AppImage"
  if [[ -x "$tool" ]]; then
    echo "$tool"
    return 0
  fi

  local url="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${arch}.AppImage"
  log "Downloading appimagetool (${arch})…"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL -o "$tool" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O "$tool" "$url"
  else
    echo "error: curl or wget required to download appimagetool" >&2
    return 1
  fi
  chmod +x "$tool"
  echo "$tool"
}

build_appimage() {
  local arch
  arch="$(appimage_host_arch)" || return 1

  local appdir="$OUT_DIR/AppDir"
  rm -rf "$appdir"
  mkdir -p "$appdir/usr"

  shopt -s dotglob
  cp -a "$STAGE"/* "$appdir/usr/"
  shopt -u dotglob

  local desktop_src="$appdir/usr/share/applications/io.github.magiskboy.easy-ssh.desktop"
  if [[ ! -f "$desktop_src" ]]; then
    echo "error: missing desktop file in stage (needed for AppImage)" >&2
    return 1
  fi
  cp -a "$desktop_src" "$appdir/io.github.magiskboy.easy-ssh.desktop"

  local icon_src=""
  for size in 256 128 512 64 48; do
    local candidate="$appdir/usr/share/icons/hicolor/${size}x${size}/apps/io.github.magiskboy.easy-ssh.png"
    if [[ -f "$candidate" ]]; then
      icon_src="$candidate"
      break
    fi
  done
  if [[ -z "$icon_src" ]]; then
    echo "error: missing hicolor icon for AppImage root" >&2
    return 1
  fi
  cp -a "$icon_src" "$appdir/io.github.magiskboy.easy-ssh.png"

  if [[ ! -x "$appdir/usr/bin/easy-ssh" ]]; then
    echo "error: AppDir missing usr/bin/easy-ssh" >&2
    return 1
  fi

  local metainfo="$appdir/usr/share/metainfo/io.github.magiskboy.easy-ssh.metainfo.xml"
  if [[ -f "$metainfo" ]]; then
    ln -sfn "io.github.magiskboy.easy-ssh.metainfo.xml" \
      "$appdir/usr/share/metainfo/io.github.magiskboy.easy-ssh.appdata.xml"
  fi

  cat >"$appdir/AppRun" <<'EOF'
#!/bin/sh
SELF="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
HERE="${SELF%/*}"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [ -d "${HERE}/usr/lib64/qt6/plugins" ]; then
  export QT_PLUGIN_PATH="${HERE}/usr/lib64/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
elif [ -d "${HERE}/usr/lib/qt6/plugins" ]; then
  export QT_PLUGIN_PATH="${HERE}/usr/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
fi
exec "${HERE}/usr/bin/easy-ssh" "$@"
EOF
  chmod +x "$appdir/AppRun"

  local tool
  tool="$(fetch_appimagetool "$arch")" || return 1

  local out_image="$OUT_DIR/easy-ssh-${TARGET}.AppImage"
  rm -f "$out_image"

  local version="${VERSION:-}"
  if [[ -z "$version" && -f "$BINARY_DIR/CMakeCache.txt" ]]; then
    version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$BINARY_DIR/CMakeCache.txt" | head -1)"
  fi

  log "Building AppImage → $out_image"
  export APPIMAGE_EXTRACT_AND_RUN=1
  ARCH="$arch" VERSION="$version" \
    "$tool" "$appdir" "$out_image"

  if [[ ! -f "$out_image" ]]; then
    echo "error: appimagetool did not produce $out_image" >&2
    return 1
  fi
  chmod +x "$out_image"
  log "Created $out_image"
  ls -lh "$out_image"

  rm -rf "$appdir"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
bundle_linux_deps

if ! build_appimage; then
  echo "error: AppImage packaging failed" >&2
  exit 1
fi

archive="easy-ssh-${TARGET}.tar.gz"
wrap="$OUT_DIR/wrap"
rm -rf "$wrap"
mkdir -p "$wrap/easy-ssh"
shopt -s dotglob
mv "$STAGE"/* "$wrap/easy-ssh/"
shopt -u dotglob
tar -C "$wrap" -czf "$OUT_DIR/$archive" easy-ssh
rm -rf "$wrap"
log "Created $OUT_DIR/$archive"
ls -lh "$OUT_DIR/$archive"
