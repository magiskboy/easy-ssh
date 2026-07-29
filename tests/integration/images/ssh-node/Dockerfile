FROM docker.io/library/alpine:3.21

RUN apk add --no-cache openssh-server openssh-sftp-server openssh-client bash \
    && ssh-keygen -A \
    && mkdir -p /run/sshd /etc/ssh/sshd_config.d

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 2222

HEALTHCHECK --interval=5s --timeout=3s --retries=10 --start-period=5s \
    CMD pgrep sshd >/dev/null || exit 1

ENTRYPOINT ["/entrypoint.sh"]
