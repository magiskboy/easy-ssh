#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Run clang-tidy in parallel on C++ translation units under src/.
#
# Requires a configured build tree with compile_commands.json, e.g.:
#   cmake --preset debug
#
# Optional:
#   EASY_SSH_BUILD_DIR   Build dir with compile_commands.json (default: ./build)
#   EASY_SSH_TIDY_FIX=1  Apply clang-tidy fixes in place
#   EASY_SSH_TIDY_JOBS   Parallel jobs (default: 0 = all CPU cores)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

BUILD_DIR="${EASY_SSH_BUILD_DIR:-${ROOT}/build}"
COMPDB="${BUILD_DIR}/compile_commands.json"
JOBS="${EASY_SSH_TIDY_JOBS:-0}"

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

RUNNER=""
for candidate in run-clang-tidy.py run-clang-tidy; do
  if command -v "${candidate}" >/dev/null 2>&1; then
    RUNNER="${candidate}"
    break
  fi
done
if [[ -z "${RUNNER}" ]]; then
  echo "error: run-clang-tidy.py not found on PATH." >&2
  echo "Install with: python -m pip install -r requirements/requirements-dev.txt" >&2
  exit 1
fi

if ! find src -type f -name '*.cpp' -print -quit | grep -q .; then
  echo "error: no .cpp sources found under src/" >&2
  exit 1
fi

args=(-p "${BUILD_DIR}" -j "${JOBS}" -quiet)
if [[ "${EASY_SSH_TIDY_FIX:-0}" == "1" ]]; then
  args+=(-fix)
fi

# Positional args are regexes matched with re.search against compile_commands paths.
echo "${RUNNER} (-j ${JOBS}, -p ${BUILD_DIR}, filter: src/.*\\.cpp\$)"
"${RUNNER}" "${args[@]}" 'src/.*\.cpp$'
echo "clang-tidy OK"
