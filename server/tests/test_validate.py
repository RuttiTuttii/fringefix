from __future__ import annotations

import copy
import json
import unittest
from pathlib import Path
from typing import Any

from fringefix_reports.validate import ReportError, validate_report

FIXTURE = Path(__file__).parent / "fixtures" / "report_v1.json"


def load() -> dict[str, Any]:
    # отчёт, собранный настоящим кодом клиента на C, а не написанный вручную
    return json.loads(FIXTURE.read_text(encoding="utf-8"))


class ValidateTest(unittest.TestCase):
    def test_client_report_is_accepted_unchanged(self) -> None:
        report = load()
        self.assertEqual(validate_report(report), report)

    def test_unknown_keys_are_dropped(self) -> None:
        report = load()
        report["username"] = "ivan"
        report["system"]["hostname"] = "PC-IVAN"
        clean = validate_report(report)
        self.assertNotIn("username", clean)
        self.assertNotIn("hostname", clean["system"])

    def test_sections_are_optional(self) -> None:
        minimal = {
            "schema": "fringefix-report",
            "version": 1,
            "app": {"version": "v0.3.0", "language": "en"},
            "comment": "",
        }
        self.assertEqual(validate_report(minimal), minimal)

    def test_rejects_wrong_schema_and_types(self) -> None:
        for patch in ({"schema": "other"}, {"version": 2}, {"app": []}):
            report = {**load(), **patch}
            with self.assertRaises(ReportError):
                validate_report(report)
        with self.assertRaises(ReportError):
            validate_report([])

    def test_rejects_paths_and_odd_names_in_games(self) -> None:
        for exe in ("C:\\Users\\Ivan\\game.exe", "Game.exe", "a/b.exe", "", "x" * 80):
            report = load()
            report["games"][0]["exe"] = exe
            with self.assertRaises(ReportError, msg=exe):
                validate_report(report)

    def test_rejects_out_of_range_numbers(self) -> None:
        cases = [
            ("settings", "sharpen", 5),
            ("settings", "gpu_priority", 99),
            ("settings", "auto_pause", True),
            ("system", "windows_build", -1),
        ]
        for section, key, value in cases:
            report = load()
            report[section][key] = value
            with self.assertRaises(ReportError, msg=f"{section}.{key}"):
                validate_report(report)
        report = load()
        report["settings"]["red"]["x"] = 2.5
        with self.assertRaises(ReportError):
            validate_report(report)

    def test_limits_on_lists_and_comment(self) -> None:
        report = load()
        report["games"] = [copy.deepcopy(report["games"][0]) for _ in range(33)]
        with self.assertRaises(ReportError):
            validate_report(report)
        report = load()
        report["errors"] = ["e"] * 6
        with self.assertRaises(ReportError):
            validate_report(report)
        report = load()
        report["comment"] = "я" * 501  # 1002 байта
        with self.assertRaises(ReportError):
            validate_report(report)

    def test_comment_control_characters_are_stripped(self) -> None:
        report = load()
        report["comment"] = "a\x00b\x1b[31mc\nd"
        self.assertEqual(validate_report(report)["comment"], "ab[31mc\nd")

    def test_error_text_must_be_printable_ascii(self) -> None:
        report = load()
        report["errors"] = ["привет"]
        with self.assertRaises(ReportError):
            validate_report(report)


if __name__ == "__main__":
    unittest.main()
