"""Сводка по сохранённым отчётам: python -m fringefix_reports.summary /var/lib/fringefix-reports"""

from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path


def summarize(root: Path) -> str:
    versions: Counter[str] = Counter()
    gpus: Counter[str] = Counter()
    games: Counter[str] = Counter()
    minutes: Counter[str] = Counter()
    errors: Counter[str] = Counter()
    total = 0
    for path in sorted(root.rglob("*.json")):
        report = json.loads(path.read_text(encoding="utf-8"))
        total += 1
        versions[report["app"]["version"]] += 1
        gpus[report.get("system", {}).get("gpu", "не указано")] += 1
        for game in report.get("games", []):
            games[game["exe"]] += game["sessions"]
            minutes[game["exe"]] += game["minutes"]
        errors.update(report.get("errors", []))
    lines = [f"отчётов: {total}"]
    for title, counter in (("версии", versions), ("видеокарты", gpus), ("ошибки", errors)):
        lines.append(f"\n{title}:")
        lines += [f"  {n:4d}  {name}" for name, n in counter.most_common(10)]
    lines.append("\nигры (сеансы, минуты):")
    lines += [f"  {n:4d}  {minutes[name]:6d}  {name}" for name, n in games.most_common(20)]
    return "\n".join(lines)


if __name__ == "__main__":
    print(summarize(Path(sys.argv[1] if len(sys.argv) > 1 else "/var/lib/fringefix-reports")))
