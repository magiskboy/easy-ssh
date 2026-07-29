#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME="$("${ROOT}/scripts/detect-container.sh")"
ENV_FILE="${ROOT}/fixtures/endpoints.env"

echo "Using container runtime: ${RUNTIME}"

chmod +x "${ROOT}/scripts/compose.sh"
"${ROOT}/scripts/compose.sh" up -d --build

wait_for_port() {
    local host="$1"
    local port="$2"
    local label="$3"
    local tries=30
    while (( tries > 0 )); do
        if (echo >/dev/tcp/"${host}"/"${port}") >/dev/null 2>&1; then
            echo "Ready: ${label} (${host}:${port})"
            return 0
        fi
        sleep 1
        tries=$((tries - 1))
    done
    echo "Timed out waiting for ${label} on ${host}:${port}" >&2
    return 1
}

wait_for_port 127.0.0.1 12222 "ssh-direct"
wait_for_port 127.0.0.1 12223 "bastion1"
wait_for_port 127.0.0.1 12224 "bastion2"

cat >"${ENV_FILE}" <<'EOF'
EASY_SSH_IT_DIRECT_HOST=127.0.0.1
EASY_SSH_IT_DIRECT_PORT=12222
EASY_SSH_IT_DIRECT_USER=direct
EASY_SSH_IT_DIRECT_PASS=passdirect

EASY_SSH_IT_BASTION1_HOST=127.0.0.1
EASY_SSH_IT_BASTION1_PORT=12223
EASY_SSH_IT_BASTION1_USER=jump1
EASY_SSH_IT_BASTION1_PASS=passapp
EASY_SSH_IT_BASTION1_GATEWAY_USER=gateway
EASY_SSH_IT_BASTION1_GATEWAY_PASS=passjump1

EASY_SSH_IT_BASTION2_HOST=127.0.0.1
EASY_SSH_IT_BASTION2_PORT=12224
EASY_SSH_IT_BASTION2_USER=jump2
EASY_SSH_IT_BASTION2_PASS=passjump2

EASY_SSH_IT_TARGET_HOST=ssh-target
EASY_SSH_IT_TARGET_PORT=22
EASY_SSH_IT_TARGET_USER=app
EASY_SSH_IT_TARGET_PASS=passapp
EOF

echo "Stack is up. Endpoints written to ${ENV_FILE}"
echo "Run integration tests: ${ROOT}/scripts/run-tests.sh"
