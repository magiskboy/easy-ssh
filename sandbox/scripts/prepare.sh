#!/usr/bin/env bash
# Generate client + host key material required before compose --build.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

./scripts/generate-keys.sh
./scripts/generate-host-keys.sh

echo "sandbox key material ready under fixtures/lab-keys and fixtures/host-keys"
