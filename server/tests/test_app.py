from __future__ import annotations

import json
import logging
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

from fringefix_reports.app import Server, Settings

FIXTURE = Path(__file__).parent / "fixtures" / "report_v1.json"


class AppTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.settings = Settings(Path(self.tmp.name), "127.0.0.1", 0, True, 3, 10_000_000)
        self.server = Server(self.settings)
        self.base = f"http://127.0.0.1:{self.server.server_address[1]}"
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.tmp.cleanup()

    def request(
        self,
        method: str,
        path: str,
        body: bytes | None = None,
        headers: dict[str, str] | None = None,
    ) -> tuple[int, dict[str, Any]]:
        req = urllib.request.Request(
            self.base + path, data=body, method=method, headers=headers or {}
        )
        try:
            with urllib.request.urlopen(req, timeout=5) as resp:
                return resp.status, json.loads(resp.read())
        except urllib.error.HTTPError as exc:
            with exc:
                return exc.code, json.loads(exc.read())

    def post(self, body: bytes, **extra: str) -> tuple[int, dict[str, Any]]:
        headers = {"Content-Type": "application/json", **extra}
        return self.request("POST", "/v1/reports", body, headers)

    def test_health_and_prefixed_path(self) -> None:
        self.assertEqual(self.request("GET", "/v1/health"), (200, {"ok": True}))
        self.assertEqual(self.request("GET", "/fringefix/api/v1/health")[0], 200)
        self.assertEqual(self.request("GET", "/nope")[0], 404)

    def test_accepts_client_report_and_stores_it(self) -> None:
        status, body = self.post(FIXTURE.read_bytes())
        self.assertEqual(status, 201)
        stored = list(Path(self.tmp.name).rglob(f"{body['id']}.json"))
        self.assertEqual(len(stored), 1)
        saved = json.loads(stored[0].read_text(encoding="utf-8"))
        self.assertEqual(saved["app"]["version"], "v0.3.0")

    def test_rejects_bad_requests(self) -> None:
        self.assertEqual(self.post(b"{not json")[0], 400)
        self.assertEqual(self.post(b"[]")[0], 422)
        self.assertEqual(self.post(b"\xff\xfe")[0], 400)
        # целое длиннее лимита int (4300 цифр) бросает ValueError в json.loads: ответ 400, а не обрыв соединения
        huge = b"1" * 5000
        self.assertEqual(self.post(huge, **{"X-Forwarded-For": "198.51.100.77"})[0], 400)
        status, _ = self.request("POST", "/v1/reports", b"{}", {"Content-Type": "text/plain"})
        self.assertEqual(status, 415)
        self.assertEqual(self.post(b"x" * 20000)[0], 413)
        other = {"Content-Type": "application/json"}
        self.assertEqual(self.request("POST", "/v1/other", b"{}", other)[0], 404)
        self.assertEqual(list(Path(self.tmp.name).rglob("*.json")), [])

    def test_rate_limit_uses_forwarded_address(self) -> None:
        payload = FIXTURE.read_bytes()
        for _ in range(3):
            self.assertEqual(self.post(payload, **{"X-Forwarded-For": "198.51.100.1"})[0], 201)
        self.assertEqual(self.post(payload, **{"X-Forwarded-For": "198.51.100.1"})[0], 429)
        # другой адрес за тем же прокси не страдает
        self.assertEqual(self.post(payload, **{"X-Forwarded-For": "198.51.100.2"})[0], 201)

    def test_no_address_in_logs_or_files(self) -> None:
        with self.assertNoLogs("fringefix_reports", level=logging.DEBUG):
            self.post(FIXTURE.read_bytes(), **{"X-Forwarded-For": "203.0.113.55"})
        for path in Path(self.tmp.name).rglob("*"):
            if path.is_file():
                self.assertNotIn("203.0.113.55", path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
