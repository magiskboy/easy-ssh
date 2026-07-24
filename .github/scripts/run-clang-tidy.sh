#!/usr/bin/env bash
# Run clang-tidy against src/*.cpp using a CMake compile database.
# Phase B: warn-only — always exits 0 after printing findings.
set -euo pipefail

BUILD_DIR="${1:?usage: run-clang-tidy.sh <build-dir>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
  echo "error: missing $BUILD_DIR/compile_commands.json" >&2
  exit 1
fi

mapfile -t sources < <(find src -type f -name '*.cpp' | sort)
if [[ ${#sources[@]} -eq 0 ]]; then
  echo "error: no .cpp files under src/" >&2
  exit 1
fi

LOG="${CLANG_TIDY_LOG:-clang-tidy.log}"
set +e
clang-tidy \
  -p "$BUILD_DIR" \
  --quiet \
  --config-file="$ROOT/.clang-tidy" \
  "${sources[@]}" \
  2>&1 | tee "$LOG"
status=${PIPESTATUS[0]}
set -e

echo "clang-tidy finished with exit code ${status} (warn-only; not failing CI)"
exit 0
