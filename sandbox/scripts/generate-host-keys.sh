#!/usr/bin/env bash
# Generate stable SSH host keys per lab role (so recreate ≠ host-key changed).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${ROOT}/fixtures/host-keys"

roles=(bastion bastion2 target sftp-off auth)

mkdir -p "${OUT}"
for role in "${roles[@]}"; do
  dir="${OUT}/${role}"
  mkdir -p "${dir}"
  if [[ -f "${dir}/ssh_host_ed25519_key" ]]; then
    echo "keep ${dir}"
    continue
  fi
  ssh-keygen -t rsa -b 3072 -N "" -f "${dir}/ssh_host_rsa_key" >/dev/null
  ssh-keygen -t ecdsa -N "" -f "${dir}/ssh_host_ecdsa_key" >/dev/null
  ssh-keygen -t ed25519 -N "" -f "${dir}/ssh_host_ed25519_key" >/dev/null
  echo "generated ${dir}"
done
