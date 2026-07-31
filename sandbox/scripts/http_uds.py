#!/usr/bin/env python3
"""Minimal HTTP server on a Unix domain socket — StreamLocal tunnel destination."""

from __future__ import annotations

import argparse
import os
import socket
from http.server import BaseHTTPRequestHandler, HTTPServer


class UnixHTTPServer(HTTPServer):
    address_family = socket.AF_UNIX

    def server_bind(self) -> None:
        path = self.server_address
        if isinstance(path, bytes):
            path = path.decode()
        if os.path.exists(path):
            os.unlink(path)
        super().server_bind()
        os.chmod(path, 0o666)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--socket", default="/run/lab/http.sock")
    parser.add_argument("--label", default="lab")
    args = parser.parse_args()
    label = args.label
    sock_path = args.socket
    os.makedirs(os.path.dirname(sock_path) or ".", exist_ok=True)

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802
            body = (
                f"easy-ssh sandbox UDS HTTP\n"
                f"label={label}\n"
                f"socket={sock_path}\n"
                f"path={self.path}\n"
            ).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt: str, *a) -> None:
            print(f"[http-uds:{label}] " + (fmt % a), flush=True)

    server = UnixHTTPServer(sock_path, Handler)
    print(f"[http-uds:{label}] listening on {sock_path}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
