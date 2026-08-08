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

# Only tidy TUs present in compile_commands.json. A blind find under src/
# also picks platform-only files (e.g. src/macos/*.cpp needs dispatch.h)
# that are not built on Linux and fail with clang-diagnostic-error.
# See: https://clang.llvm.org/docs/JSONCompilationDatabase.html
files=()
while IFS= read -r line; do
  [[ -n "$line" ]] || continue
  files+=("$line")
done < <(
  python3 - "$COMPDB" "$ROOT" <<'PY'
import json
import sys
from pathlib import Path

compdb = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
seen: set[str] = set()
ordered: list[str] = []
for entry in json.loads(compdb.read_text(encoding="utf-8")):
    raw = entry.get("file") or ""
    path = Path(raw)
    if not path.is_absolute():
        path = (Path(entry.get("directory") or ".") / path)
    path = path.resolve()
    # Stale compile_commands.json (e.g. after renames) may still list
    # removed TUs; clang-tidy then fails with clang-diagnostic-error.
    if not path.is_file():
        continue
    try:
        rel = path.relative_to(root).as_posix()
    except ValueError:
        continue
    if not (rel.startswith("src/") and rel.endswith(".cpp")):
        continue
    if rel in seen:
        continue
    seen.add(rel)
    ordered.append(rel)
for rel in sorted(ordered):
    print(rel)
PY
)
if [[ ${#files[@]} -eq 0 ]]; then
  echo "error: no src/**/*.cpp entries found in ${COMPDB}" >&2
  exit 1
fi

args=(-p "${BUILD_DIR}" --quiet)
if [[ "${EASY_SSH_TIDY_FIX:-0}" == "1" ]]; then
  args+=(--fix)
fi

echo "clang-tidy (${#files[@]} files, -p ${BUILD_DIR})"
clang-tidy "${args[@]}" "${files[@]}"
echo "clang-tidy OK"
