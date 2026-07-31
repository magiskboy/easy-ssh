#!/usr/bin/env python3
"""Minimal HTTP server on TCP — tunnel / SOCKS destination."""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--label", default="lab")
    args = parser.parse_args()
    label = args.label

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802
            body = (
                f"easy-ssh sandbox HTTP\n"
                f"label={label}\n"
                f"path={self.path}\n"
                f"host={self.headers.get('Host', '')}\n"
            ).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt: str, *a) -> None:
            print(f"[http-tcp:{label}] " + (fmt % a), flush=True)

    server = ThreadingHTTPServer((args.bind, args.port), Handler)
    print(f"[http-tcp:{label}] listening on {args.bind}:{args.port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
