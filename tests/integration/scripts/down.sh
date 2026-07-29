#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
chmod +x "${ROOT}/scripts/compose.sh"
"${ROOT}/scripts/compose.sh" down -v --remove-orphans
echo "Integration stack stopped."
