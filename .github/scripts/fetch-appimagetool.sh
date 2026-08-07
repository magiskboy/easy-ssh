#!/usr/bin/env bash

# Download a pinned appimagetool release (not available via pip).
# Usage: fetch-appimagetool.sh <output-dir>
# Prints the absolute path of the tool on stdout.
set -euo pipefail

# Pin matches https://github.com/AppImage/appimagetool/releases — not on PyPI.
APPIMAGETOOL_VERSION="${APPIMAGETOOL_VERSION:-1.9.1}"

out_dir="${1:?output directory}"
mkdir -p "$out_dir"

arch="$(uname -m)"
case "$arch" in
  x86_64|amd64) tool_arch="x86_64" ;;
  aarch64|arm64) tool_arch="aarch64" ;;
  *)
    echo "unsupported arch for appimagetool: $arch" >&2
    exit 1
    ;;
esac

tool="${out_dir}/appimagetool-${tool_arch}.AppImage"
url="https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/appimagetool-${tool_arch}.AppImage"

if [[ ! -x "$tool" ]]; then
  curl -fsSL "$url" -o "$tool"
  chmod +x "$tool"
fi

# CPack looks for "appimagetool" on PATH by default.
link="${out_dir}/appimagetool"
ln -sfn "$(basename "$tool")" "$link"

printf '%s\n' "$tool"
