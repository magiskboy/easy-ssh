#!/usr/bin/env bash
# Generate lab SSH keypair used by pubkey auth users (keyuser / keyonly).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEYS_DIR="${ROOT}/keys"
FIXTURES="${ROOT}/fixtures/lab-keys"

mkdir -p "${KEYS_DIR}" "${FIXTURES}"

if [[ ! -f "${KEYS_DIR}/id_ed25519" ]]; then
  ssh-keygen -t ed25519 -N "" -C "easy-ssh-sandbox-lab" -f "${KEYS_DIR}/id_ed25519"
  chmod 600 "${KEYS_DIR}/id_ed25519"
  chmod 644 "${KEYS_DIR}/id_ed25519.pub"

  # Passphrase-protected copy for P5 passphrase UX tests.
  ssh-keygen -t ed25519 -N "lab-passphrase" -C "easy-ssh-sandbox-passphrase" \
    -f "${KEYS_DIR}/id_ed25519_passphrase"
  chmod 600 "${KEYS_DIR}/id_ed25519_passphrase"

  # Combined authorized_keys (both public keys accepted on lab hosts).
  cat "${KEYS_DIR}/id_ed25519.pub" "${KEYS_DIR}/id_ed25519_passphrase.pub" \
    >"${KEYS_DIR}/authorized_keys"
  chmod 644 "${KEYS_DIR}/authorized_keys"
else
  echo "reusing existing keys in ${KEYS_DIR}"
fi

cp -a "${KEYS_DIR}/id_ed25519" "${KEYS_DIR}/id_ed25519.pub" \
  "${KEYS_DIR}/id_ed25519_passphrase" "${KEYS_DIR}/id_ed25519_passphrase.pub" \
  "${KEYS_DIR}/authorized_keys" "${FIXTURES}/"

cat <<EOF
Generated / synced:
  ${KEYS_DIR}/id_ed25519              (no passphrase)
  ${KEYS_DIR}/id_ed25519_passphrase   (passphrase: lab-passphrase)
  ${KEYS_DIR}/authorized_keys         (optional bind-mount)
  ${FIXTURES}/                        (baked into image on next --build)

Password for all lab users: easy
EOF
