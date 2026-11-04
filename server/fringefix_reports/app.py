"""HTTP-интерфейс приёма отчётов: POST /v1/reports и GET /v1/health."""

from __future__ import annotations

import json
import logging
import os
from dataclasses import dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

from .ratelimit import RateLimiter
from .store import ReportStore, StoreFullError
from .validate import ReportError, validate_report

MAX_BODY = 16 * 1024
PREFIX = "/fringefix/api"
log = logging.getLogger("fringefix_reports")


@dataclass(frozen=True)
class Settings:
    directory: Path
    bind: str
    port: int
    trust_proxy: bool
    rate_per_hour: int
    max_total_bytes: int

    @classmethod
    def from_env(cls) -> Settings:
        env = os.environ
        return cls(
            directory=Path(env.get("FRINGEFIX_REPORTS_DIR", "/var/lib/fringefix-reports")),
            bind=env.get("FRINGEFIX_BIND", "127.0.0.1"),
            port=int(env.get("FRINGEFIX_PORT", "8791")),
            trust_proxy=env.get("FRINGEFIX_TRUST_PROXY", "1") == "1",
            rate_per_hour=int(env.get("FRINGEFIX_RATE_PER_HOUR", "5")),
            max_total_bytes=int(env.get("FRINGEFIX_MAX_TOTAL_MB", "200")) * 1024 * 1024,
        )


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, settings: Settings) -> None:
        super().__init__((settings.bind, settings.port), Handler)
        self.settings = settings
        self.store = ReportStore(settings.directory, settings.max_total_bytes)
        self.limiter = RateLimiter(settings.rate_per_hour)


class Handler(BaseHTTPRequestHandler):
    server: Server
    # обрыв медленного клиента: слот потока не должен занимать тот, кто молчит
    timeout = 10
    server_version = "fringefix-reports"
    sys_version = ""

    def log_message(self, format: str, *args: Any) -> None:  # noqa: A002
        # стандартный лог пишет адрес клиента и путь, нам нужно, чтобы адресов не было нигде
        return

    def _path(self) -> str:
        path = self.path.split("?", 1)[0]
        return path[len(PREFIX) :] if path.startswith(PREFIX + "/") else path

    def _client(self) -> str:
        if self.server.settings.trust_proxy:
            forwarded = self.headers.get("X-Forwarded-For", "")
            if forwarded:
                return forwarded.split(",")[0].strip()
        return self.client_address[0]

    def _send(
        self,
        status: HTTPStatus,
        body: dict[str, Any],
        headers: dict[str, str] | None = None,
    ) -> None:
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        for name, value in (headers or {}).items():
            self.send_header(name, value)
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self) -> None:  # noqa: N802
        if self._path() == "/v1/health":
            self._send(HTTPStatus.OK, {"ok": True})
        else:
            self._send(HTTPStatus.NOT_FOUND, {"error": "not_found"})

    def do_POST(self) -> None:  # noqa: N802
        if self._path() != "/v1/reports":
            self._send(HTTPStatus.NOT_FOUND, {"error": "not_found"})
            return
        if not self.headers.get("Content-Type", "").startswith("application/json"):
            self._send(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, {"error": "content_type"})
            return
        length = self.headers.get("Content-Length", "")
        if not length.isdigit():
            self._send(HTTPStatus.LENGTH_REQUIRED, {"error": "length_required"})
            return
        if int(length) > MAX_BODY:
            self._send(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, {"error": "too_large"})
            return
        client = self._client()
        if not self.server.limiter.allow(client):
            wait = str(self.server.limiter.retry_after(client))
            self._send(
                HTTPStatus.TOO_MANY_REQUESTS, {"error": "rate_limited"}, {"Retry-After": wait}
            )
            return
        self._accept(self.rfile.read(int(length)))

    def _accept(self, body: bytes) -> None:
        try:
            report = validate_report(json.loads(body.decode("utf-8")))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._send(HTTPStatus.BAD_REQUEST, {"error": "bad_json"})
            return
        except ReportError as exc:
            self._send(HTTPStatus.UNPROCESSABLE_ENTITY, {"error": exc.code})
            return
        try:
            report_id = self.server.store.save(report)
        except StoreFullError:
            self._send(HTTPStatus.INSUFFICIENT_STORAGE, {"error": "storage_full"})
            return
        self._send(HTTPStatus.CREATED, {"id": report_id})


def run(settings: Settings | None = None) -> None:
    logging.basicConfig(level=logging.WARNING, format="%(levelname)s %(message)s")
    server = Server(settings or Settings.from_env())
    log.warning("слушаю %s:%d", server.settings.bind, server.settings.port)
    try:
        server.serve_forever()
    finally:
        server.server_close()
