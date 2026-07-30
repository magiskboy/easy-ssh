#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Add / refresh SPDX copyright + license headers on given files.
#
# Usage:
#   .github/scripts/run-reuse-annotate.sh path/to/file.cpp […]
#   .github/scripts/run-reuse-annotate.sh $(git diff --name-only --cached -- '*.cpp' '*.h')
#
# Defaults (no args): all C/C++ under src/
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if ! command -v reuse >/dev/null 2>&1; then
  echo "error: reuse not found on PATH." >&2
  echo "Install with: python -m pip install -r requirements/requirements-dev.txt" >&2
  exit 1
fi

COPYRIGHT="${EASY_SSH_COPYRIGHT:-Nguyen Khac Thanh <ask@nkthanh.dev>}"
YEAR="${EASY_SSH_COPYRIGHT_YEAR:-$(date +%Y)}"
LICENSE="${EASY_SSH_SPDX_LICENSE:-GPL-3.0-only}"

if [[ "$#" -eq 0 ]]; then
  mapfile -t files < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
else
  files=("$@")
fi

if [[ ${#files[@]} -eq 0 ]]; then
  echo "error: no files to annotate" >&2
  exit 1
fi

reuse annotate \
  --copyright="$COPYRIGHT" \
  --copyright-prefix=spdx-string-c \
  --year="$YEAR" \
  --license="$LICENSE" \
  "${files[@]}"

echo "Annotated ${#files[@]} file(s) ($LICENSE)"
