#!/usr/bin/env bash
# Package the built binary for upload / release.
set -euo pipefail

TARGET="${1:?usage: package.sh <target-triple>}"
BINARY_DIR="${2:?usage: package.sh <target> <binary-dir>}"
OUT_DIR="${3:-dist}"

mkdir -p "$OUT_DIR"
STAGE="$OUT_DIR/stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"

if [[ -f "$BINARY_DIR/easy-ssh.exe" ]]; then
  cp "$BINARY_DIR/easy-ssh.exe" "$STAGE/"
  ARCHIVE="easy-ssh-${TARGET}.zip"
  (
    cd "$STAGE"
    if command -v 7z >/dev/null 2>&1; then
      7z a -tzip "../$ARCHIVE" ./*
    else
      powershell -NoProfile -Command "Compress-Archive -Path * -DestinationPath '../$ARCHIVE'"
    fi
  )
elif [[ -f "$BINARY_DIR/easy-ssh" ]]; then
  cp "$BINARY_DIR/easy-ssh" "$STAGE/"
  ARCHIVE="easy-ssh-${TARGET}.tar.gz"
  tar -C "$STAGE" -czf "$OUT_DIR/$ARCHIVE" .
else
  echo "error: easy-ssh binary not found in $BINARY_DIR" >&2
  ls -la "$BINARY_DIR" >&2 || true
  exit 1
fi

echo "Created $OUT_DIR/$ARCHIVE"
ls -lh "$OUT_DIR/$ARCHIVE"
