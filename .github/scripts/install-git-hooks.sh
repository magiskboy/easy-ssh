#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
#
# SPDX-License-Identifier: GPL-3.0-only

# Enable repository git hooks (clang-format + REUSE pre-commit).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

chmod +x .githooks/pre-commit
chmod +x .github/scripts/run-clang-format.sh
chmod +x .github/scripts/run-reuse-lint.sh

git config core.hooksPath .githooks

echo "Git hooks installed (core.hooksPath=.githooks)"
echo "Pre-commit runs:"
echo "  - clang-format on staged *.cpp / *.h (auto-fix + re-stage)"
echo "  - reuse lint (whole tree; same as CI)"
echo
echo "Required on PATH:"
echo "  clang-format  →  python3 -m pip install -r requirements/requirements.txt"
echo "  reuse         →  python3 -m pip install -r requirements/requirements-dev.txt"
echo
echo "Tip: after installing into .deps/venv:"
echo "  export PATH=\"\$(pwd)/.deps/venv/bin:\$PATH\""
echo
echo "CI still runs clang-format --check as a blocking gate."