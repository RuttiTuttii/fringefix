from __future__ import annotations

import json
import os
import stat
import tempfile
import unittest
from pathlib import Path

from fringefix_reports.ratelimit import RateLimiter
from fringefix_reports.store import ReportStore, StoreFullError


class RateLimitTest(unittest.TestCase):
    def test_blocks_after_limit_and_frees_after_window(self) -> None:
        limiter = RateLimiter(limit=2, window=100.0)
        self.assertTrue(limiter.allow("1.2.3.4", now=0))
        self.assertTrue(limiter.allow("1.2.3.4", now=1))
        self.assertFalse(limiter.allow("1.2.3.4", now=2))
        self.assertTrue(limiter.allow("5.6.7.8", now=2))
        self.assertTrue(limiter.allow("1.2.3.4", now=101))
        self.assertGreaterEqual(limiter.retry_after("1.2.3.4", now=102), 1)

    def test_addresses_are_not_kept_in_memory(self) -> None:
        limiter = RateLimiter(limit=1)
        limiter.allow("203.0.113.7")
        dump = repr(vars(limiter))
        self.assertNotIn("203.0.113.7", dump)


class StoreTest(unittest.TestCase):
    def test_saves_with_month_only_name_and_private_mode(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            store = ReportStore(Path(tmp), 1_000_000)
            report_id = store.save({"comment": "привет", "n": 1})
            files = list(Path(tmp).rglob("*.json"))
            self.assertEqual(len(files), 1)
            self.assertEqual(files[0].stem, report_id)
            self.assertRegex(report_id, r"^\d{6}-[0-9a-f]{12}$")
            self.assertEqual(stat.S_IMODE(os.stat(files[0]).st_mode), 0o600)
            self.assertEqual(json.loads(files[0].read_text(encoding="utf-8"))["comment"], "привет")
            self.assertEqual(list(Path(tmp).rglob(".*.tmp")), [])

    def test_total_size_cap(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            store = ReportStore(Path(tmp), 200)
            store.save({"a": 1})
            with self.assertRaises(StoreFullError):
                store.save({"data": "x" * 500})

    def test_total_is_recounted_on_restart(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            ReportStore(Path(tmp), 1_000_000).save({"a": 1})
            self.assertGreater(ReportStore(Path(tmp), 1_000_000).total_bytes, 0)


if __name__ == "__main__":
    unittest.main()
