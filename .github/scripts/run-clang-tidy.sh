#!/usr/bin/env bash

# Run clang-tidy on C++ translation units under src/.
#
# Requires a configured build tree with compile_commands.json, e.g.:
#   cmake --preset debug
#
# Optional:
#   EASY_SSH_BUILD_DIR   Build dir with compile_commands.json (default: ./build)
#   EASY_SSH_TIDY_FIX=1  Apply clang-tidy fixes in place
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

BUILD_DIR="${EASY_SSH_BUILD_DIR:-${ROOT}/build}"
COMPDB="${BUILD_DIR}/compile_commands.json"

if [[ ! -f "$COMPDB" ]]; then
  echo "error: ${COMPDB} not found." >&2
  echo "Configure first, e.g.: cmake --preset debug" >&2
  echo "Or set EASY_SSH_BUILD_DIR to a configured build directory." >&2
  exit 1
fi

if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "error: clang-tidy not found on PATH." >&2
  echo "Install with: python -m pip install -r requirements/requirements-dev.txt" >&2
  exit 1
fi

# Portable alternative to mapfile (macOS /bin/bash 3.2).
files=()
while IFS= read -r line; do
  files+=("$line")
done < <(find src -type f -name '*.cpp' | sort)
if [[ ${#files[@]} -eq 0 ]]; then
  echo "error: no .cpp sources found under src/" >&2
  exit 1
fi

args=(-p "${BUILD_DIR}" --quiet)
if [[ "${EASY_SSH_TIDY_FIX:-0}" == "1" ]]; then
  args+=(--fix)
fi

echo "clang-tidy (${#files[@]} files, -p ${BUILD_DIR})"
clang-tidy "${args[@]}" "${files[@]}"
echo "clang-tidy OK"
