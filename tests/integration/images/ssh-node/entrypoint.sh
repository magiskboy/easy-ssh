#!/bin/bash
set -euo pipefail

SSH_PORT="${SSH_PORT:-2222}"

mkdir -p /run/sshd /var/empty

cat >/etc/ssh/sshd_config.d/easy-ssh-it.conf <<EOF
Port ${SSH_PORT}
ListenAddress 0.0.0.0
PermitRootLogin no
PasswordAuthentication yes
KbdInteractiveAuthentication no
ChallengeResponseAuthentication no
UsePAM no
X11Forwarding no
PrintMotd no
AcceptEnv LANG LC_*
Subsystem sftp /usr/lib/ssh/sftp-server
AllowTcpForwarding yes
GatewayPorts no
EOF

if [[ -n "${SSH_USERS:-}" ]]; then
    IFS=';' read -ra ENTRIES <<< "${SSH_USERS}"
    for entry in "${ENTRIES[@]}"; do
        [[ -z "${entry}" ]] && continue
        user="${entry%%:*}"
        pass="${entry#*:}"
        if ! id "${user}" >/dev/null 2>&1; then
            adduser -D -s /bin/sh "${user}"
        fi
        echo "${user}:${pass}" | chpasswd
    done
fi

exec /usr/sbin/sshd -D -e
