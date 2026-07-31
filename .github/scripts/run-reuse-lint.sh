#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Verify the tree against the REUSE Specification (SPDX headers / REUSE.toml).
# sandbox/ is a local lab fixture tree and is intentionally out of scope.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if ! command -v reuse >/dev/null 2>&1; then
  echo "error: reuse not found on PATH." >&2
  echo "Install with: python -m pip install -r requirements/requirements-dev.txt" >&2
  exit 1
fi

mapfile -t files < <(git ls-files | grep -v '^sandbox/')
if [[ ${#files[@]} -eq 0 ]]; then
  echo "error: no files to lint" >&2
  exit 1
fi

reuse lint-file "${files[@]}"
