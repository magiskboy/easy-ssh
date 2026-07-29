#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${ROOT}/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO}/build-integration}"
ENV_FILE="${ROOT}/fixtures/endpoints.env"

if [[ ! -f "${ENV_FILE}" ]]; then
    echo "Missing ${ENV_FILE}. Start the stack first:" >&2
    echo "  ${ROOT}/scripts/up.sh" >&2
    exit 1
fi

# shellcheck disable=SC1090
set -a
source "${ENV_FILE}"
set +a

if ! (echo >/dev/tcp/127.0.0.1/12222) >/dev/null 2>&1; then
    echo "Integration stack does not appear to be running on 127.0.0.1:12222." >&2
    echo "Run: ${ROOT}/scripts/up.sh" >&2
    exit 1
fi

cmake -S "${REPO}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DEASY_SSH_INTEGRATION_TESTS=ON

cmake --build "${BUILD_DIR}" --target easy-ssh-integration-tests -j"$(nproc)"

echo "Running integration tests..."
cd "${BUILD_DIR}"
ctest -R integration-tests --output-on-failure
