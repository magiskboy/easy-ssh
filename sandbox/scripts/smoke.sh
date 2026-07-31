#!/usr/bin/env bash
# Non-interactive smoke checks against a running lab (compose up).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEY="${ROOT}/fixtures/lab-keys/id_ed25519"
cfg="$(mktemp)"
trap 'rm -f "$cfg"' EXIT

cat >"$cfg" <<EOF
Host *
  StrictHostKeyChecking no
  UserKnownHostsFile /dev/null
  GlobalKnownHostsFile /dev/null
  IdentityFile ${KEY}
  IdentitiesOnly yes
  LogLevel ERROR
EOF

pass() { printf '  OK  %s\n' "$*"; }
fail() { printf ' FAIL %s\n' "$*"; exit 1; }

echo "easy-ssh sandbox smoke"
sshpass -p easy ssh -F "$cfg" -p 2200 -o PreferredAuthentications=password \
  -o IdentityFile=/dev/null -o IdentitiesOnly=yes easy@127.0.0.1 'true' \
  && pass "password → target:2200" || fail "password"

ssh -F "$cfg" -p 2200 -o PreferredAuthentications=publickey keyuser@127.0.0.1 'true' \
  && pass "pubkey → target:2200" || fail "pubkey"

ssh -F "$cfg" -o ProxyJump=keyuser@127.0.0.1:2222 keyuser@target 'true' \
  && pass "ProxyJump 1-hop" || fail "jump"

ssh -F "$cfg" -o ProxyJump=keyuser@127.0.0.1:2222,keyuser@bastion2:22 keyuser@target 'true' \
  && pass "ProxyJump 2-hop" || fail "multi-hop"

ssh -F "$cfg" -p 2202 keyonly@127.0.0.1 'true' \
  && pass "keyonly → target-auth:2202" || fail "keyonly"

sshpass -p easy ssh -F "$cfg" -p 2202 -o PreferredAuthentications=password \
  -o PubkeyAuthentication=no -o IdentityFile=/dev/null passonly@127.0.0.1 'true' \
  && pass "passonly → target-auth:2202" || fail "passonly"

sshpass -p easy ssh -F "$cfg" -p 2201 -o PreferredAuthentications=password \
  -o IdentityFile=/dev/null easy@127.0.0.1 'true' \
  && pass "shell → target-sftp-off:2201" || fail "sftp-off"

curl -fsS http://127.0.0.1:18080/ >/dev/null && pass "HTTP target:18080" || fail "http-target"
curl -fsS http://127.0.0.1:18081/ >/dev/null && pass "HTTP http-backend:18081" || fail "http-backend"

sshpass -p easy ssh -F "$cfg" -p 2200 -o PreferredAuthentications=password \
  -o IdentityFile=/dev/null easy@127.0.0.1 \
  'curl -fsS --unix-socket /run/lab/http.sock http://x/' >/dev/null \
  && pass "UDS HTTP /run/lab/http.sock" || fail "uds"

echo "all smoke checks passed"
