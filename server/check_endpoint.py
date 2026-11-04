"""Проверка живого приёмника отчётов снаружи, тем же путём, которым ходит клиент.

python3 check_endpoint.py https://moscow.fragmentzal.store/fringefix/api [--full]

По умолчанию: health и один принятый отчёт (расходует 1 из лимита 5 в час на адрес).
--full: ещё отказы на битый JSON, неверную схему, неверный тип, слишком большое тело
(расходует до 3 из лимита). Тестовый отчёт помечен комментарием endpoint-check, его можно удалить.
"""

from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

MARK = "endpoint-check"
FIXTURE = Path(__file__).parent / "tests" / "fixtures" / "report_v1.json"


def call(
    base: str, method: str, path: str, body: bytes | None = None, ctype: str | None = None
) -> tuple[int, dict[str, Any]]:
    headers = {"Content-Type": ctype} if ctype else {}
    req = urllib.request.Request(base + path, data=body, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return resp.status, json.loads(resp.read())
    except urllib.error.HTTPError as exc:
        with exc:
            try:
                return exc.code, json.loads(exc.read())
            except json.JSONDecodeError:
                return exc.code, {}


Reply = tuple[int, dict[str, Any]]


def check(base: str, full: bool) -> list[tuple[str, bool, str]]:
    results: list[tuple[str, bool, str]] = []
    json_type = "application/json"

    def expect(name: str, got: Reply, status: int, key: str | None = None) -> None:
        ok = got[0] == status and (key is None or key in got[1])
        results.append((name, ok, f"{got[0]} {got[1]}"))

    def post(body: bytes, ctype: str) -> Reply:
        return call(base, "POST", "/v1/reports", body, ctype)

    expect("health", call(base, "GET", "/v1/health"), 200, "ok")
    report = json.loads(FIXTURE.read_text(encoding="utf-8"))
    report["comment"] = MARK
    body = json.dumps(report, ensure_ascii=False).encode("utf-8")
    expect("accepts a valid report", post(body, json_type), 201, "id")
    if full:
        expect("rejects wrong content type", post(b"{}", "text/plain"), 415)
        expect("rejects an oversized body", post(b"x" * 20000, json_type), 413)
        expect("rejects broken json", post(b"{oops", json_type), 400)
        expect("rejects a wrong schema", post(b"{}", json_type), 422)
    return results


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    results = check(args[0].rstrip("/"), "--full" in sys.argv)
    for name, ok, detail in results:
        print(f"{'ok  ' if ok else 'FAIL'} {name}: {detail}")
    return 0 if all(ok for _, ok, _ in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
