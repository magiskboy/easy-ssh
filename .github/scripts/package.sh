#!/usr/bin/env bash
# Package a self-contained Easy SSH tree for upload / release.
# Usage: package.sh <target-triple> <binary-dir> [out-dir]
set -euo pipefail

TARGET="${1:?usage: package.sh <target-triple> <binary-dir> [out-dir]}"
BINARY_DIR="${2:?usage: package.sh <target> <binary-dir> [out-dir]}"
OUT_DIR="${3:-dist}"

mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
BINARY_DIR="$(cd "$BINARY_DIR" && pwd)"

STAGE="$OUT_DIR/stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"

log() { printf 'package: %s\n' "$*" >&2; }

# ---------------------------------------------------------------------------
# Install project into the stage prefix (runs Qt deploy install(SCRIPT)).
# ---------------------------------------------------------------------------
log "Installing into $STAGE"
cmake --install "$BINARY_DIR" --prefix "$STAGE"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
find_binary() {
  if [[ -x "$STAGE/bin/easy-ssh" ]]; then
    echo "$STAGE/bin/easy-ssh"
  elif [[ -x "$STAGE/easy-ssh" ]]; then
    echo "$STAGE/easy-ssh"
  elif [[ -f "$STAGE/bin/easy-ssh.exe" ]]; then
    echo "$STAGE/bin/easy-ssh.exe"
  elif [[ -f "$STAGE/easy-ssh.exe" ]]; then
    echo "$STAGE/easy-ssh.exe"
  else
    return 1
  fi
}

stage_libdir() {
  # Prefer Libraries= from qt.conf written by Qt deploy.
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
    # Always skip hard system/loader libs.
    ld-linux*.so*|libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*)
      return 1 ;;
    libstdc++.so*|libgcc_s.so*|libc++.so*|libc++abi.so*)
      return 1 ;;
    # App + Qt runtime.
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
  local plugins_dest
  # Match qt.conf Plugins= when present.
  local conf="$STAGE/bin/qt.conf"
  local plugins_rel="lib64/qt6/plugins"
  if [[ -f "$conf" ]]; then
    local p
    p="$(sed -n 's/^Plugins *= *//p' "$conf" | head -1 | tr -d '\r')"
    [[ -n "$p" ]] && plugins_rel="$p"
  elif [[ "$(basename "$libdir")" == "lib" ]]; then
    plugins_rel="lib/qt6/plugins"
  fi
  plugins_dest="$STAGE/$plugins_rel"

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
  bin="$(find_binary)" || {
    echo "error: easy-ssh binary not found under $STAGE after install" >&2
    find "$STAGE" -maxdepth 3 -type f | head -50 >&2 || true
    exit 1
  }

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
    local lib
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

  # Ensure qt.conf Prefix/Libraries/Plugins are coherent with what we produced.
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

bundle_macos_deps() {
  local app=""
  local apps=("$STAGE"/*.app)
  if [[ -d "${apps[0]:-}" ]]; then
    app="${apps[0]}"
  else
    echo "error: .app bundle not found under $STAGE" >&2
    ls -la "$STAGE" >&2 || true
    exit 1
  fi

  local macdeployqt=""
  if command -v macdeployqt >/dev/null 2>&1; then
    macdeployqt="$(command -v macdeployqt)"
  elif [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    local p
    IFS=':' read -ra _prefixes <<<"$CMAKE_PREFIX_PATH"
    for p in "${_prefixes[@]}"; do
      if [[ -x "$p/bin/macdeployqt" ]]; then
        macdeployqt="$p/bin/macdeployqt"
        break
      fi
    done
  fi

  local -a libpaths=()
  if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    local p
    IFS=':' read -ra _prefixes <<<"$CMAKE_PREFIX_PATH"
    for p in "${_prefixes[@]}"; do
      [[ -d "$p/lib" ]] && libpaths+=(-libpath="$p/lib")
    done
  fi

  if [[ -n "$macdeployqt" ]]; then
    log "Running macdeployqt on $app"
    "$macdeployqt" "$app" "${libpaths[@]}" -always-overwrite || true
  else
    log "warning: macdeployqt not found; relying on CMake deploy script only"
  fi

  local bin
  bin="$(find "$app/Contents/MacOS" -type f -perm -111 | head -1 || true)"
  local frameworks="$app/Contents/Frameworks"
  mkdir -p "$frameworks"

  copy_macos_lib() {
    local src="$1"
    local base
    base="$(basename "$src")"
    [[ -e "$frameworks/$base" ]] && return 0
    cp -a "$src" "$frameworks/"
    log "  + $base"
    if command -v install_name_tool >/dev/null 2>&1 && [[ -n "$bin" ]]; then
      install_name_tool -change "$src" "@executable_path/../Frameworks/$base" "$bin" 2>/dev/null || true
      local id
      id="$(otool -D "$frameworks/$base" 2>/dev/null | tail -1 | awk '{print $1}')"
      if [[ -n "$id" ]]; then
        install_name_tool -id "@executable_path/../Frameworks/$base" "$frameworks/$base" 2>/dev/null || true
        install_name_tool -change "$id" "@executable_path/../Frameworks/$base" "$bin" 2>/dev/null || true
      fi
    fi
  }

  if [[ -n "$bin" ]]; then
    local dep
    while IFS= read -r dep; do
      dep="${dep#	}"
      dep="$(awk '{print $1}' <<<"$dep")"
      [[ -e "$dep" ]] || continue
      case "$(basename "$dep")" in
        libssh*.dylib|libqtermwidget*.dylib|*qt*keychain*.dylib|Qt*.framework|libQt6*.dylib)
          if [[ "$dep" == *".framework"* ]]; then
            continue
          fi
          copy_macos_lib "$dep"
          ;;
      esac
    done < <(otool -L "$bin" 2>/dev/null | tail -n +2 || true)
  fi
}

bundle_windows_deps() {
  local exe
  exe="$(find_binary)" || {
    echo "error: easy-ssh.exe not found under $STAGE" >&2
    exit 1
  }

  local windeployqt=""
  if command -v windeployqt >/dev/null 2>&1; then
    windeployqt="$(command -v windeployqt)"
  elif command -v windeployqt6 >/dev/null 2>&1; then
    windeployqt="$(command -v windeployqt6)"
  fi

  if [[ -n "$windeployqt" ]]; then
    log "Running windeployqt on $exe"
    "$windeployqt" --release --no-translations "$exe" || \
      "$windeployqt" "$exe" || true
  else
    log "warning: windeployqt not found; relying on CMake deploy script only"
  fi

  local -a names=(
    ssh.dll libssh.dll
    qtermwidget6.dll qt6keychain.dll qtkeychain.dll
    # libssh transitive deps (vcpkg / MSVC naming)
    libcrypto-3-x64.dll libssl-3-x64.dll
    libcrypto-1_1-x64.dll libssl-1_1-x64.dll
    zlib1.dll zlib.dll
  )
  local dest
  dest="$(dirname "$exe")"

  copy_named_dlls_from() {
    local root="$1"
    [[ -d "$root" ]] || return 0
    local name found
    for name in "${names[@]}"; do
      [[ -f "$dest/$name" ]] && continue
      found="$(find "$root" -name "$name" 2>/dev/null | head -1 || true)"
      if [[ -n "$found" ]]; then
        cp -a "$found" "$dest/"
        log "  + $name"
      fi
    done
  }

  if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    local p
    # Support both CMake (;) and Unix (:) separators.
    local paths="${CMAKE_PREFIX_PATH//;/$'\n'}"
    paths="${paths//:/$'\n'}"
    while IFS= read -r p; do
      [[ -n "$p" ]] || continue
      copy_named_dlls_from "$p"
    done <<<"$paths"
  fi

  if [[ -n "${VCPKG_ROOT:-}" ]]; then
    copy_named_dlls_from "${VCPKG_ROOT}/installed/x64-windows"
  fi

  if [[ -n "${PREFIX:-}" ]]; then
    copy_named_dlls_from "$PREFIX"
  fi
}

# ---------------------------------------------------------------------------
# AppImage (Linux)
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

  # Map our install prefix into the classic AppDir usr/ layout.
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

  # appimagetool still looks for the older *.appdata.xml name.
  local metainfo="$appdir/usr/share/metainfo/io.github.magiskboy.easy-ssh.metainfo.xml"
  if [[ -f "$metainfo" ]]; then
    ln -sfn "io.github.magiskboy.easy-ssh.metainfo.xml" \
      "$appdir/usr/share/metainfo/io.github.magiskboy.easy-ssh.appdata.xml"
  fi

  cat >"$appdir/AppRun" <<'EOF'
#!/bin/sh
# AppImage entrypoint — keep Qt/plugins relocatable next to the binary.
SELF="$(readlink -f "$0" 2>/dev/null || realpath "$0" 2>/dev/null || echo "$0")"
HERE="${SELF%/*}"
export PATH="${HERE}/usr/bin:${PATH}"
# Prefer bundled libs over host Qt when both exist.
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
  # CI runners often lack FUSE; extract-and-run avoids mounting the tool AppImage.
  export APPIMAGE_EXTRACT_AND_RUN=1
  # appimagetool reads ARCH for the runtime; VERSION is optional metadata.
  ARCH="$arch" VERSION="$version" \
    "$tool" "$appdir" "$out_image"

  if [[ ! -f "$out_image" ]]; then
    echo "error: appimagetool did not produce $out_image" >&2
    return 1
  fi
  chmod +x "$out_image"
  log "Created $out_image"
  ls -lh "$out_image"

  # Drop intermediate AppDir to keep dist/ small; keep tools cached.
  rm -rf "$appdir"
}

# ---------------------------------------------------------------------------
# Archive
# ---------------------------------------------------------------------------
archive_linux() {
  # AppImage first while $STAGE is still intact.
  if ! build_appimage; then
    echo "error: AppImage packaging failed" >&2
    exit 1
  fi

  local archive="easy-ssh-${TARGET}.tar.gz"
  local wrap="$OUT_DIR/wrap"
  rm -rf "$wrap"
  mkdir -p "$wrap/easy-ssh"
  shopt -s dotglob
  mv "$STAGE"/* "$wrap/easy-ssh/"
  shopt -u dotglob
  tar -C "$wrap" -czf "$OUT_DIR/$archive" easy-ssh
  rm -rf "$wrap"
  log "Created $OUT_DIR/$archive"
  ls -lh "$OUT_DIR/$archive"
}

archive_macos() {
  local archive="easy-ssh-${TARGET}.dmg"
  local app=""
  local apps=("$STAGE"/*.app)
  if [[ -d "${apps[0]:-}" ]]; then
    app="${apps[0]}"
  else
    echo "error: no .app to package" >&2
    exit 1
  fi

  local macdeployqt=""
  if command -v macdeployqt >/dev/null 2>&1; then
    macdeployqt="$(command -v macdeployqt)"
  fi

  if [[ -n "$macdeployqt" ]]; then
    log "Creating DMG via macdeployqt"
    rm -f "${app%.app}.dmg"
    if "$macdeployqt" "$app" -dmg -always-overwrite; then
      local produced="${app%.app}.dmg"
      if [[ -f "$produced" ]]; then
        mv "$produced" "$OUT_DIR/$archive"
        log "Created $OUT_DIR/$archive"
        ls -lh "$OUT_DIR/$archive"
        return 0
      fi
    fi
  fi

  if command -v hdiutil >/dev/null 2>&1; then
    log "Creating DMG via hdiutil"
    local vol="$OUT_DIR/dmg-root"
    rm -rf "$vol"
    mkdir -p "$vol"
    cp -R "$app" "$vol/"
    ln -s /Applications "$vol/Applications"
    rm -f "$OUT_DIR/$archive"
    hdiutil create -volname "Easy SSH" -srcfolder "$vol" -ov -format UDZO "$OUT_DIR/$archive"
    rm -rf "$vol"
    log "Created $OUT_DIR/$archive"
    ls -lh "$OUT_DIR/$archive"
    return 0
  fi

  archive="easy-ssh-${TARGET}.tar.gz"
  tar -C "$STAGE" -czf "$OUT_DIR/$archive" "$(basename "$app")"
  log "Created $OUT_DIR/$archive (tar.gz fallback)"
  ls -lh "$OUT_DIR/$archive"
}

archive_windows() {
  local nsis
  nsis="$(find "$BINARY_DIR" -maxdepth 2 \( -name 'easy-ssh-*.exe' -o -name '*-win64.exe' \) 2>/dev/null | head -1 || true)"
  if [[ -n "$nsis" ]]; then
    local dest="easy-ssh-${TARGET}-setup.exe"
    cp -a "$nsis" "$OUT_DIR/$dest"
    log "Created $OUT_DIR/$dest"
    ls -lh "$OUT_DIR/$dest"
    return 0
  fi

  local archive="easy-ssh-${TARGET}.zip"
  local wrap="$OUT_DIR/wrap"
  rm -rf "$wrap"
  mkdir -p "$wrap/easy-ssh"
  shopt -s dotglob
  mv "$STAGE"/* "$wrap/easy-ssh/"
  shopt -u dotglob
  (
    cd "$wrap"
    if command -v 7z >/dev/null 2>&1; then
      7z a -tzip "$OUT_DIR/$archive" easy-ssh
    elif command -v zip >/dev/null 2>&1; then
      zip -r "$OUT_DIR/$archive" easy-ssh
    else
      powershell -NoProfile -Command "Compress-Archive -Path 'easy-ssh' -DestinationPath '$OUT_DIR/$archive'"
    fi
  )
  rm -rf "$wrap"
  log "Created $OUT_DIR/$archive"
  ls -lh "$OUT_DIR/$archive"
}

case "$(uname -s)" in
  Linux)
    bundle_linux_deps
    archive_linux
    ;;
  Darwin)
    bundle_macos_deps
    archive_macos
    ;;
  MINGW*|MSYS*|CYGWIN*)
    bundle_windows_deps
    archive_windows
    ;;
  *)
    if find_binary 2>/dev/null | grep -q '\.exe$'; then
      bundle_windows_deps
      archive_windows
    elif compgen -G "$STAGE/*.app" >/dev/null; then
      bundle_macos_deps
      archive_macos
    else
      bundle_linux_deps
      archive_linux
    fi
    ;;
esac
