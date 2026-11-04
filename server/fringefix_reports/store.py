"""Хранилище отчётов: по файлу на отчёт, имя только с месяцем, без времени и адресов."""

from __future__ import annotations

import json
import os
import secrets
import threading
from datetime import UTC, datetime
from pathlib import Path
from typing import Any


class StoreFullError(Exception):
    """Достигнут общий лимит размера хранилища."""


class ReportStore:
    def __init__(self, root: Path, max_total_bytes: int) -> None:
        self._root = root
        self._max_total = max_total_bytes
        self._lock = threading.Lock()
        root.mkdir(parents=True, exist_ok=True)
        self._total = sum(p.stat().st_size for p in root.rglob("*.json"))

    @property
    def total_bytes(self) -> int:
        return self._total

    def save(self, report: dict[str, Any]) -> str:
        """Пишет отчёт и возвращает его номер вида 202611-a1b2c3d4e5f6."""
        data = json.dumps(report, ensure_ascii=False, indent=2).encode("utf-8")
        month = datetime.now(UTC).strftime("%Y%m")
        report_id = f"{month}-{secrets.token_hex(6)}"
        folder = self._root / f"{month[:4]}-{month[4:]}"
        with self._lock:
            if self._total + len(data) > self._max_total:
                raise StoreFullError("хранилище заполнено")
            folder.mkdir(parents=True, exist_ok=True)
            tmp = folder / f".{report_id}.tmp"
            # 0600: читать отчёты может только владелец сервиса
            fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(fd, "wb") as f:
                f.write(data)
            tmp.rename(folder / f"{report_id}.json")
            self._total += len(data)
        return report_id
