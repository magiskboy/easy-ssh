#!/usr/bin/env bash
# Build an AppImage from a cmake --install staging prefix using appimagetool.
set -euo pipefail

staging="${1:?staging prefix}"
out="${2:?output .AppImage path}"
appimagetool="${3:?appimagetool path}"
workdir="${4:-$(mktemp -d)}"
appdir="${workdir}/AppDir"

rm -rf "$appdir"
mkdir -p "$appdir/usr"
cp -a "$staging"/. "$appdir/usr/"

desktop_src="$appdir/usr/share/applications/io.github.magiskboy.easy-ssh.desktop"
icon_src="$appdir/usr/share/icons/hicolor/256x256/apps/io.github.magiskboy.easy-ssh.png"
[[ -f "$desktop_src" ]] || { echo "missing desktop file: $desktop_src" >&2; exit 1; }
[[ -f "$icon_src" ]] || { echo "missing icon: $icon_src" >&2; exit 1; }
[[ -x "$appdir/usr/bin/easy-ssh" ]] || { echo "missing binary: $appdir/usr/bin/easy-ssh" >&2; exit 1; }

cp "$desktop_src" "$appdir/io.github.magiskboy.easy-ssh.desktop"
cp "$icon_src" "$appdir/io.github.magiskboy.easy-ssh.png"

cat > "$appdir/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib64:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/lib/qt6/plugins:${HERE}/usr/plugins:${QT_PLUGIN_PATH:-}"
exec "${HERE}/usr/bin/easy-ssh" "$@"
EOF
chmod +x "$appdir/AppRun"

mkdir -p "$(dirname "$out")"
export APPIMAGE_EXTRACT_AND_RUN=1
"$appimagetool" "$appdir" "$out"
echo "Wrote $out"
