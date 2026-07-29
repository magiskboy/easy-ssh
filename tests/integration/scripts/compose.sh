#!/usr/bin/env bash
# Run compose with podman-compose (preferred) or docker compose.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME="$("${ROOT}/scripts/detect-container.sh")"

export COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME:-easy-ssh-it}"

compose_cmd() {
    if [[ "${RUNTIME}" == "podman" ]] && command -v podman-compose >/dev/null 2>&1; then
        podman-compose -f "${ROOT}/docker-compose.yml" "$@"
        return
    fi

    if [[ "${RUNTIME}" == "podman" ]] && podman compose version >/dev/null 2>&1; then
        podman compose -f "${ROOT}/docker-compose.yml" "$@"
        return
    fi

    if command -v docker >/dev/null 2>&1; then
        docker compose -f "${ROOT}/docker-compose.yml" "$@"
        return
    fi

    echo "No podman-compose, podman compose, or docker compose available." >&2
    exit 1
}

compose_cmd "$@"
