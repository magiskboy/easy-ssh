#!/usr/bin/env bash
# Package a self-contained Easy SSH .app / .dmg for macOS.
# Usage: package-macos.sh <target-triple> <binary-dir> [out-dir]
set -euo pipefail

TARGET="${1:?usage: package-macos.sh <target-triple> <binary-dir> [out-dir]}"
BINARY_DIR="${2:?usage: package-macos.sh <target> <binary-dir> [out-dir]}"
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
# Bundle dependencies
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Archive
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
bundle_macos_deps
archive_macos
