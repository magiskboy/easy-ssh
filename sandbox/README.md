# easy-ssh integration sandbox

Lab containers that approximate a real SSH estate so you can exercise easy-ssh end-to-end: shell, SFTP/SCP, Local/Remote/Dynamic(+UDS) tunnels, ProxyJump, and the auth matrix. Base image is **Ubuntu 26.04 LTS**.

## Topology

```text
  easy-ssh (host)
       │
       ├─ :2200 ──▶ target          shell + SFTP + HTTP:8080 + /run/lab/http.sock
       ├─ :2201 ──▶ target-sftp-off SFTP disabled (SCP fallback)
       ├─ :2202 ──▶ target-auth     password / pubkey / kbdint users
       ├─ :2222 ──▶ bastion ──┬──▶ target | bastion2 | http-backend  (ProxyJump)
       │                      └──▶ bastion2 ──▶ target               (2-hop)
       ├─ :18080 ─▶ target:8080     direct HTTP (optional)
       └─ :18081 ─▶ http-backend    separate tunnel destination
```

All published ports bind to `127.0.0.1` only. Containers share the `easy-ssh-lab` bridge network (DNS names: `bastion`, `bastion2`, `target`, …).

## Quick start

Requires Podman + Compose plugin, or Docker Compose v2+.

```bash
cd sandbox
chmod +x scripts/*.sh
./scripts/prepare.sh                 # generate client + host keys (gitignored)
podman compose -f compose.yaml up -d --build
```

If you previously connected to the lab with ephemeral host keys, clear stale
entries: `ssh-keygen -R '[127.0.0.1]:2200'` (and `:2201` `:2202` `:2222`).

Tear down:

```bash
podman compose -f compose.yaml down
# add -v only if you added named volumes later
```

Default password for every lab user: **`easy`** (override with `LAB_PASSWORD` in the environment).

## Credentials

| User | Host(s) | Auth | Notes |
|------|---------|------|--------|
| `easy` | all SSH hosts | password `easy`, or pubkey | General smoke tests |
| `keyuser` | all SSH hosts | pubkey (or password) | Key file: `fixtures/lab-keys/id_ed25519` |
| `keyonly` | `target-auth` only | pubkey only | Same key files |
| `passonly` | `target-auth` only | password only | No authorized_keys |
| `kbdint` | `target-auth` only | keyboard-interactive | Password still `easy` |

Key material is **generated locally** (not committed). `./scripts/prepare.sh` writes:

| Path | Use |
|------|-----|
| `fixtures/lab-keys/id_ed25519` | Private key, no passphrase |
| `fixtures/lab-keys/id_ed25519_passphrase` | Private key, passphrase `lab-passphrase` |
| `fixtures/lab-keys/authorized_keys` | Installed for lab users at container start |
| `fixtures/host-keys/<role>/` | Stable server host keys (baked into image) |
| `keys/` | Same client keys for optional bind-mount |

Rebuild the image after regenerating keys (`compose … --build`).

## Feature → host map

| Feature | How to test against this lab |
|---------|------------------------------|
| Shell / multi-shell | Connect `easy@127.0.0.1:2200` |
| SFTP | Same; browse `/home/easy/sandbox-data` |
| SCP fallback | `easy@127.0.0.1:2201` (`target-sftp-off`) |
| Local TCP tunnel | Local `127.0.0.1:<L>` → remote `127.0.0.1:8080` on `target`; `curl 127.0.0.1:<L>` |
| Local → remote UDS | Local TCP → remote `/run/lab/http.sock` on `target` |
| Dynamic SOCKS5 | SOCKS bind on host → dest `http-backend:8080` or `target:8080` (via tunnel session on `target`) |
| Remote TCP | Remote listen on `target` (`GatewayPorts clientspecified`); peer curls published/forwarded port |
| ProxyJump 1-hop | Host=`target` Port=`22`, Jump=`easy@127.0.0.1:2222`, password `easy` |
| ProxyJump 2-hop | Host=`target` Port=`22`, Jump=`easy@127.0.0.1:2222,easy@bastion2:22` |
| Auth matrix | `127.0.0.1:2202` with users above |
| Agent login (P5) | `ssh-add fixtures/lab-keys/id_ed25519` then connect as `keyonly` / `keyuser` with agent |
| Agent Forwarding (P6, when built) | Same + enable forward; on remote `ssh-add -l` |
| ProxyCommand (P4, when built) | e.g. `nc %h %p` to `127.0.0.1` `%p`=2200 — no extra container |

Sample OpenSSH config for import / comparison: [`fixtures/ssh-config.sample`](fixtures/ssh-config.sample).

## Port reference

| Host port | Service | Container |
|-----------|---------|-----------|
| `2222` | SSH | bastion |
| `2200` | SSH | target |
| `2201` | SSH | target-sftp-off |
| `2202` | SSH | target-auth |
| `18080` | HTTP | target:8080 |
| `18081` | HTTP | http-backend:8080 |

`bastion2` has **no** host port — only reachable as a jump hop by DNS name `bastion2` from inside the lab network.

## Smoke checks (CLI)

```bash
./scripts/smoke.sh
```

Or manually (use a throwaway ssh config so ProxyJump inherits `StrictHostKeyChecking`):

```bash
ssh -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -o IdentityFile=fixtures/lab-keys/id_ed25519 -o IdentitiesOnly=yes \
  -p 2200 keyuser@127.0.0.1 'hostname'

# Jump (prefer ./scripts/smoke.sh or fixtures/ssh-config.sample)
ssh -F fixtures/ssh-config.sample lab-via-bastion 'hostname'
```

## Layout

```text
sandbox/
  compose.yaml
  Containerfile              # ubuntu:26.04 + OpenSSH + helpers
  scripts/
    prepare.sh               # generate client + host keys
    entrypoint.sh            # ROLE-based sshd + HTTP/UDS
    generate-keys.sh
    generate-host-keys.sh
    smoke.sh
    http_tcp.py / http_uds.py
  sshd/                      # per-role sshd snippets
  fixtures/
    lab-keys/                # client keys (generated, gitignored)
    host-keys/               # stable server host keys (generated, gitignored)
    ssh-config.sample
  keys/                      # client key copy for bind-mount (gitignored)
```

## Security note

This lab is **intentionally insecure** (shared password, locally generated keys,
agent forwarding on). Use only on localhost. Do not expose these ports on a
public interface. Key files under `fixtures/` and `keys/` must not be committed.
