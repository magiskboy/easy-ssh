#!/usr/bin/env bash
# Create .deps/venv and install requirements.txt into it.
# Prints the venv scripts/bin directory (add to PATH).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VENV="${EASY_SSH_VENV:-${ROOT}/.deps/venv}"
REQ="${1:-${ROOT}/requirements.txt}"

if command -v python3 >/dev/null 2>&1; then
  PY=python3
elif command -v python >/dev/null 2>&1; then
  PY=python
else
  echo "python3/python not found" >&2
  exit 1
fi

"$PY" -m venv "$VENV"

if [[ -d "${VENV}/Scripts" ]]; then
  BIN="${VENV}/Scripts"
  PIP_PY="${BIN}/python.exe"
else
  BIN="${VENV}/bin"
  PIP_PY="${BIN}/python"
fi

"$PIP_PY" -m pip install --upgrade pip >&2
"$PIP_PY" -m pip install -r "$REQ" >&2

# GITHUB_PATH / native Windows tools need a Windows path, not Git Bash /d/...
if command -v cygpath >/dev/null 2>&1; then
  BIN="$(cygpath -w "$BIN")"
fi

printf '%s\n' "$BIN"
