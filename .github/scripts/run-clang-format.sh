#!/usr/bin/env bash
# Apply .clang-format to all C++ sources under src/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

mapfile -t files < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
if [[ ${#files[@]} -eq 0 ]]; then
  echo "error: no C++ sources found under src/" >&2
  exit 1
fi

clang-format -i "${files[@]}"
echo "clang-format applied (${#files[@]} files)"
