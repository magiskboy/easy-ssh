#!/usr/bin/env bash

# Remove artifacts produced by local configure / build / package / IDE indexing.
#
# Usage:
#   ./scripts/cleanup.sh              # build trees, staging, dist, caches, …
#   ./scripts/cleanup.sh --deps       # also .deps (aqt Qt, pip venv, tools)
#   ./scripts/cleanup.sh --all        # --deps plus root .venv
#   ./scripts/cleanup.sh --dry-run    # print paths only (combine with other flags)
#
# Does not delete sources, committed resources/, or third_party/ trees.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

DRY_RUN=0
CLEAN_DEPS=0
CLEAN_VENV=0

usage() {
  cat <<'EOF'
Remove artifacts produced by local configure / build / package / IDE indexing.

Usage:
  ./scripts/cleanup.sh              # build trees, staging, dist, caches, …
  ./scripts/cleanup.sh --deps       # also .deps (aqt Qt, pip venv, tools)
  ./scripts/cleanup.sh --all        # --deps plus root .venv
  ./scripts/cleanup.sh --dry-run    # print paths only (combine with other flags)

Does not delete sources, committed resources/, or third_party/ trees.
EOF
  exit "${1:-0}"
}

for arg in "$@"; do
  case "$arg" in
    -h | --help) usage 0 ;;
    --dry-run) DRY_RUN=1 ;;
    --deps) CLEAN_DEPS=1 ;;
    --all)
      CLEAN_DEPS=1
      CLEAN_VENV=1
      ;;
    *)
      echo "error: unknown option: $arg" >&2
      usage 1
      ;;
  esac
done

remove() {
  local path="$1"
  if [[ ! -e "$path" && ! -L "$path" ]]; then
    return 0
  fi
  if [[ "$DRY_RUN" -eq 1 ]]; then
    if [[ -d "$path" && ! -L "$path" ]]; then
      echo "would remove dir  $path"
    else
      echo "would remove file $path"
    fi
    return 0
  fi
  rm -rf -- "$path"
  echo "removed $path"
}

# --- Always: CMake / CPack / clangd / stray compile DB ---

# Preset binary dirs (debug → build/, release → build-release/, …)
shopt -s nullglob
for d in build build-* cmake-build-* out; do
  # Only top-level dirs; skip if somehow a file
  [[ -d "$d" ]] || continue
  remove "$d"
done
shopt -u nullglob

remove dist
remove staging

# compile_commands.json may sit at repo root (CMake Tools copy) or be a symlink
remove compile_commands.json

remove aqtinstall.log
remove .cache

# CPack leftovers when run from the repo root (normally under dist/ / build-*/)
shopt -s nullglob
for f in *.deb *.rpm *.AppImage *.dmg *.exe easy-ssh-*.tar.gz; do
  remove "$f"
done
shopt -u nullglob

# --- Optional: downloaded toolchains / Qt ---

if [[ "$CLEAN_DEPS" -eq 1 ]]; then
  remove .deps
fi

if [[ "$CLEAN_VENV" -eq 1 ]]; then
  remove .venv
fi

if [[ "$DRY_RUN" -eq 1 ]]; then
  echo "dry-run complete (nothing deleted)"
else
  echo "cleanup complete"
fi
