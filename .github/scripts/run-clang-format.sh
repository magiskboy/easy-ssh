#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Format C++ sources with .clang-format.
#
# Usage:
#   .github/scripts/run-clang-format.sh [files…]           # apply in place
#   .github/scripts/run-clang-format.sh --check [files…]   # dry-run; fail if needed
#
# With no file arguments, formats every *.cpp / *.h under src/ (CI default).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CHECK=0
files=()

for arg in "$@"; do
  case "$arg" in
    -h | --help)
      cat <<'EOF'
Format C++ sources with .clang-format.

Usage:
  .github/scripts/run-clang-format.sh [files…]           # apply in place
  .github/scripts/run-clang-format.sh --check [files…]   # dry-run; fail if needed

With no file arguments, formats every *.cpp / *.h under src/.
EOF
      exit 0
      ;;
    --check) CHECK=1 ;;
    -*)
      echo "error: unknown option: $arg" >&2
      echo "usage: $0 [--check] [files…]" >&2
      exit 1
      ;;
    *) files+=("$arg") ;;
  esac
done

if ! command -v clang-format >/dev/null 2>&1; then
  echo "error: clang-format not found on PATH." >&2
  echo "Install with: python -m pip install -r requirements/requirements.txt" >&2
  exit 1
fi

if [[ ${#files[@]} -eq 0 ]]; then
  mapfile -t files < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "error: no C++ sources found under src/" >&2
    exit 1
  fi
fi

# Drop paths that no longer exist (e.g. deleted in the same commit).
existing=()
for f in "${files[@]}"; do
  if [[ -f "$f" ]]; then
    existing+=("$f")
  fi
done

if [[ ${#existing[@]} -eq 0 ]]; then
  echo "clang-format: no existing files to check"
  exit 0
fi

if [[ "$CHECK" -eq 1 ]]; then
  clang-format --dry-run --Werror "${existing[@]}"
  echo "clang-format OK (${#existing[@]} files)"
else
  clang-format -i "${existing[@]}"
  echo "clang-format applied (${#existing[@]} files)"
fi
