#!/usr/bin/env bash

# Generate platform icons from icon.png (source of truth at repo root).
# Requires ImageMagick (magick or convert). macOS .icns: iconutil (Darwin) or
# python3 + icnsutil as a fallback for committing assets from Linux.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${ROOT}/icon.png"
APP_ID="io.github.magiskboy.easy-ssh"

if [[ ! -f "$SRC" ]]; then
  echo "error: missing $SRC" >&2
  exit 1
fi

if command -v magick >/dev/null 2>&1; then
  IM=(magick)
elif command -v convert >/dev/null 2>&1; then
  IM=(convert)
else
  echo "error: ImageMagick (magick/convert) is required" >&2
  exit 1
fi

resize() {
  local size="$1"
  local out="$2"
  mkdir -p "$(dirname "$out")"
  "${IM[@]}" "$SRC" -resize "${size}x${size}" -strip PNG32:"$out"
}

echo "Generating runtime icon…"
resize 256 "${ROOT}/resources/icons/app-256.png"

echo "Generating Linux hicolor icons…"
for size in 16 22 24 32 48 64 128 256 512; do
  resize "$size" \
    "${ROOT}/resources/linux/icons/hicolor/${size}x${size}/apps/${APP_ID}.png"
done

echo "Generating Windows .ico…"
mkdir -p "${ROOT}/resources/windows"
ICO_STAGING="${ROOT}/resources/windows/.ico-staging"
rm -rf "$ICO_STAGING"
mkdir -p "$ICO_STAGING"
for size in 16 32 48 256; do
  resize "$size" "${ICO_STAGING}/icon-${size}.png"
done
"${IM[@]}" \
  "${ICO_STAGING}/icon-16.png" \
  "${ICO_STAGING}/icon-32.png" \
  "${ICO_STAGING}/icon-48.png" \
  "${ICO_STAGING}/icon-256.png" \
  "${ROOT}/resources/windows/easy-ssh.ico"
rm -rf "$ICO_STAGING"

# Windows VERSIONINFO comes from resources/windows/easy-ssh.rc.in
# (configured by CMake from PROJECT_VERSION). Do not emit a duplicate .rc here.

echo "Generating macOS .icns…"
ICNS_OUT="${ROOT}/resources/macos/easy-ssh.icns"
mkdir -p "${ROOT}/resources/macos"
ICONSET="${ROOT}/resources/macos/easy-ssh.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"

# iconutil expects specific names; also useful as source for icnsutil.
for pair in \
  "16:icon_16x16.png" \
  "32:icon_16x16@2x.png" \
  "32:icon_32x32.png" \
  "64:icon_32x32@2x.png" \
  "128:icon_128x128.png" \
  "256:icon_128x128@2x.png" \
  "256:icon_256x256.png" \
  "512:icon_256x256@2x.png" \
  "512:icon_512x512.png" \
  "1024:icon_512x512@2x.png"
do
  size="${pair%%:*}"
  name="${pair##*:}"
  "${IM[@]}" "$SRC" -resize "${size}x${size}" -strip PNG32:"${ICONSET}/${name}"
done

if [[ "$(uname -s)" == "Darwin" ]] && command -v iconutil >/dev/null 2>&1; then
  iconutil -c icns "$ICONSET" -o "$ICNS_OUT"
  rm -rf "$ICONSET"
elif python3 -c "import icnsutil" 2>/dev/null; then
  python3 - "$ICONSET" "$ICNS_OUT" <<'PY'
import sys
from pathlib import Path
import icnsutil

iconset, out = Path(sys.argv[1]), Path(sys.argv[2])
img = icnsutil.IcnsFile()
# Map iconset filenames to ICNS media types via add_media(file=...).
for path in sorted(iconset.iterdir()):
    if path.suffix.lower() == ".png":
        img.add_media(file=path)
img.write(out)
print(f"Wrote {out} via icnsutil")
PY
  rm -rf "$ICONSET"
else
  rm -rf "$ICONSET"
  echo "error: cannot build .icns — need iconutil (macOS) or python3 package icnsutil" >&2
  echo "       pip install icnsutil   # or regenerate on macOS" >&2
  exit 1
fi

echo "Done."
echo "  resources/icons/app-256.png"
echo "  resources/linux/icons/hicolor/*/apps/${APP_ID}.png"
echo "  resources/windows/easy-ssh.ico"
echo "  resources/macos/easy-ssh.icns"
