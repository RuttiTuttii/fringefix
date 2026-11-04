"""Ограничение частоты по адресу клиента; сами адреса не хранятся, только соль и хеш."""

from __future__ import annotations

import hashlib
import os
import threading
import time


class RateLimiter:
    """Не больше `limit` запросов на ключ за скользящее окно `window` секунд, всё в памяти."""

    def __init__(self, limit: int, window: float = 3600.0) -> None:
        self._limit = limit
        self._window = window
        # соль живёт только в памяти процесса: по дампу хеш не вернуть к адресу
        self._salt = os.urandom(16)
        self._hits: dict[bytes, list[float]] = {}
        self._lock = threading.Lock()

    def _key(self, client: str) -> bytes:
        return hashlib.sha256(self._salt + client.encode("utf-8", "replace")).digest()

    def allow(self, client: str, now: float | None = None) -> bool:
        """Учитывает попытку и говорит, пропускать ли её."""
        moment = time.monotonic() if now is None else now
        key = self._key(client)
        with self._lock:
            hits = [t for t in self._hits.get(key, []) if moment - t < self._window]
            if len(hits) >= self._limit:
                self._hits[key] = hits
                return False
            hits.append(moment)
            self._hits[key] = hits
            self._drop_stale(moment)
            return True

    def retry_after(self, client: str, now: float | None = None) -> int:
        """Через сколько секунд освободится место, не меньше одной."""
        moment = time.monotonic() if now is None else now
        with self._lock:
            hits = self._hits.get(self._key(client), [])
            if not hits:
                return 1
            return max(1, int(self._window - (moment - min(hits))) + 1)

    def _drop_stale(self, moment: float) -> None:
        # чистка редкая: иначе память росла бы по числу разных клиентов
        if len(self._hits) < 1024:
            return
        for key in [k for k, v in self._hits.items() if not v or moment - v[-1] >= self._window]:
            del self._hits[key]
