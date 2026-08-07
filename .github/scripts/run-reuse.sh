#!/usr/bin/env bash

# REUSE / SPDX helpers for Easy SSH.
#
# In-file SPDX headers apply only to C / C++ / Objective-C / Objective-C++ /
# Swift under src/ and tests/. Other tracked files use REUSE.toml annotations.
# sandbox/ is intentionally out of scope for lint.
#
# Usage:
#   .github/scripts/run-reuse.sh lint
#   .github/scripts/run-reuse.sh annotate [files…]
#   .github/scripts/run-reuse.sh annotate --year=2026 path/to/File.cpp
#
# annotate with no file args: all matching sources under src/ and tests/
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

COPYRIGHT="${EASY_SSH_COPYRIGHT:-Nguyen Khac Thanh <ask@nkthanh.dev>}"
YEAR="${EASY_SSH_COPYRIGHT_YEAR:-$(date +%Y)}"
LICENSE="${EASY_SSH_SPDX_LICENSE:-GPL-3.0-only}"

SOURCE_FIND_EXPR=(
  -type f \(
    -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx'
    -o -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx'
    -o -name '*.m' -o -name '*.mm'
    -o -name '*.swift'
  \)
)

usage() {
  cat <<'EOF'
REUSE / SPDX helpers for Easy SSH.

Usage:
  .github/scripts/run-reuse.sh lint
  .github/scripts/run-reuse.sh annotate [options] [files…]
  .github/scripts/run-reuse.sh -h | --help

Commands:
  lint       Verify tracked files (excludes sandbox/) against REUSE / SPDX
  annotate   Add / refresh SPDX headers on src/ and tests/ language sources

annotate options:
  --copyright=NAME   Copyright holder (default: $EASY_SSH_COPYRIGHT or project author)
  --year=YEAR        Copyright year (default: $EASY_SSH_COPYRIGHT_YEAR or current year)
  --license=SPDX     SPDX license id (default: $EASY_SSH_SPDX_LICENSE or GPL-3.0-only)

With no file arguments, annotate covers every matching source under src/ and tests/.
Only C/C++/ObjC/ObjC++/Swift under those trees may be annotated; use REUSE.toml for others.
EOF
}

require_reuse() {
  if ! command -v reuse >/dev/null 2>&1; then
    echo "error: reuse not found on PATH." >&2
    echo "Install with: python -m pip install -r requirements/requirements-dev.txt" >&2
    exit 1
  fi
}

is_allowed_source() {
  case "$1" in
    src/*|tests/*) ;;
    *) return 1 ;;
  esac
  case "$1" in
    *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx|*.m|*.mm|*.swift) return 0 ;;
    *) return 1 ;;
  esac
}

cmd_lint() {
  if [[ "$#" -ne 0 ]]; then
    echo "error: lint does not take file arguments" >&2
    echo "usage: $0 lint" >&2
    exit 1
  fi

  require_reuse

  files=()
  while IFS= read -r line; do
    [[ -f "$line" ]] || continue
    files+=("$line")
  done < <(git ls-files | grep -v '^sandbox/')

  if [[ ${#files[@]} -eq 0 ]]; then
    echo "error: no files to lint" >&2
    exit 1
  fi

  reuse lint-file "${files[@]}"
}

cmd_annotate() {
  files=()
  for arg in "$@"; do
    case "$arg" in
      --copyright=*)
        COPYRIGHT="${arg#--copyright=}"
        ;;
      --year=*)
        YEAR="${arg#--year=}"
        ;;
      --license=*)
        LICENSE="${arg#--license=}"
        ;;
      -h | --help)
        usage
        exit 0
        ;;
      --*)
        echo "error: unknown annotate option: $arg" >&2
        echo "usage: $0 annotate [--copyright=NAME] [--year=YEAR] [--license=SPDX] [files…]" >&2
        exit 1
        ;;
      *)
        files+=("$arg")
        ;;
    esac
  done

  require_reuse

  if [[ ${#files[@]} -eq 0 ]]; then
    while IFS= read -r line; do
      files+=("$line")
    done < <(
      {
        find src "${SOURCE_FIND_EXPR[@]}"
        find tests "${SOURCE_FIND_EXPR[@]}"
      } | sort
    )
  fi

  if [[ ${#files[@]} -eq 0 ]]; then
    echo "error: no files to annotate" >&2
    exit 1
  fi

  recognised=()
  objc=()
  for f in "${files[@]}"; do
    if ! is_allowed_source "$f"; then
      echo "error: refusing to annotate file outside src/tests language sources: $f" >&2
      echo "Only C/C++/ObjC/ObjC++/Swift under src/ or tests/ get in-file headers; use REUSE.toml for others." >&2
      exit 1
    fi
    case "$f" in
      *.m|*.mm) objc+=("$f") ;;
      *) recognised+=("$f") ;;
    esac
  done

  annotate() {
    reuse annotate \
      --copyright="$COPYRIGHT" \
      --copyright-prefix=spdx-string-c \
      --year="$YEAR" \
      --license="$LICENSE" \
      "$@"
  }

  total=0
  if [[ ${#recognised[@]} -gt 0 ]]; then
    annotate "${recognised[@]}"
    total=$((total + ${#recognised[@]}))
  fi
  # reuse does not map .m / .mm; force C-style block comments to match existing headers.
  if [[ ${#objc[@]} -gt 0 ]]; then
    annotate --style=c "${objc[@]}"
    total=$((total + ${#objc[@]}))
  fi

  echo "Annotated ${total} file(s) ($LICENSE)"
}

if [[ "$#" -eq 0 ]]; then
  echo "error: missing command" >&2
  usage >&2
  exit 1
fi

case "$1" in
  -h | --help)
    usage
    exit 0
    ;;
  lint)
    shift
    cmd_lint "$@"
    ;;
  annotate)
    shift
    cmd_annotate "$@"
    ;;
  *)
    echo "error: unknown command: $1" >&2
    usage >&2
    exit 1
    ;;
esac
