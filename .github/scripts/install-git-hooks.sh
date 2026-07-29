#!/usr/bin/env bash
# Enable repository git hooks (clang-format pre-commit).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

chmod +x .githooks/pre-commit
chmod +x .github/scripts/run-clang-format-check.sh

git config core.hooksPath .githooks

echo "Git hooks installed (core.hooksPath=.githooks)"
echo "Pre-commit runs: .github/scripts/run-clang-format-check.sh"
echo
echo "Tip: CI uses clang-format 22.x. Match locally with:"
echo "  python3 -m pip install --user 'clang-format==22.1.8'"
