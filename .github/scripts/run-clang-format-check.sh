#!/usr/bin/env bash
# Fail if any C++ source under src/ differs from .clang-format.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

mapfile -t files < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
if [[ ${#files[@]} -eq 0 ]]; then
  echo "error: no C++ sources found under src/" >&2
  exit 1
fi

clang-format --dry-run --Werror "${files[@]}"
echo "clang-format OK (${#files[@]} files)"
