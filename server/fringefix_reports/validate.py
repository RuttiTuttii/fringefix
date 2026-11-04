"""Проверка отчёта по белому списку: в хранилище попадает только то, что перечислено здесь."""

from __future__ import annotations

import re
from typing import Any

MAX_COMMENT_BYTES = 1000
MAX_GAMES = 32
MAX_ERRORS = 5
UINT_MAX = 2**32 - 1

_VERSION_RE = re.compile(r"^[A-Za-z0-9._+-]{1,32}$")
_EXE_RE = re.compile(r"^[a-z0-9._+ -]{1,63}$")
_PRINTABLE_RE = re.compile(r"^[\x20-\x7e]{1,96}$")
_HOWS = {"fullscreen", "3d", "listed"}
_ACTIONS = {"pause", "light"}
_LANGUAGES = {"ru", "en"}


class ReportError(Exception):
    """Отчёт не прошёл проверку: code идёт в ответ клиенту, message для логов и тестов."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(f"{code}: {message}")
        self.code = code
        self.message = message


def _fail(code: str, message: str) -> ReportError:
    return ReportError(code, message)


def _obj(value: Any, name: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise _fail("bad_type", f"{name} должен быть объектом")
    return value


def _uint(value: Any, name: str) -> int:
    # bool это подкласс int, но true вместо числа считаем ошибкой
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= UINT_MAX:
        raise _fail("bad_value", f"{name} должен быть целым от 0 до {UINT_MAX}")
    return value


def _ranged_int(value: Any, name: str, lo: int, hi: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not lo <= value <= hi:
        raise _fail("bad_value", f"{name} должен быть целым от {lo} до {hi}")
    return value


def _ranged_float(value: Any, name: str, lo: float, hi: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise _fail("bad_value", f"{name} должен быть числом")
    if not lo <= float(value) <= hi:
        raise _fail("bad_value", f"{name} должен быть от {lo} до {hi}")
    return float(value)


def _text(value: Any, name: str, pattern: re.Pattern[str]) -> str:
    if not isinstance(value, str) or not pattern.match(value):
        raise _fail("bad_value", f"{name} имеет недопустимый вид")
    return value


def _app(raw: dict[str, Any]) -> dict[str, Any]:
    app = _obj(raw.get("app"), "app")
    language = app.get("language")
    if language not in _LANGUAGES:
        raise _fail("bad_value", "app.language должен быть ru или en")
    return {"version": _text(app.get("version"), "app.version", _VERSION_RE), "language": language}


def _system(raw: dict[str, Any]) -> dict[str, Any]:
    system = _obj(raw, "system")
    monitor = _obj(system.get("monitor"), "system.monitor")
    gpu = system.get("gpu")
    if not isinstance(gpu, str) or len(gpu) > 64 or any(ord(c) < 32 for c in gpu):
        raise _fail("bad_value", "system.gpu имеет недопустимый вид")
    hdr = monitor.get("hdr")
    if not isinstance(hdr, bool):
        raise _fail("bad_value", "system.monitor.hdr должен быть true или false")
    return {
        "windows_build": _ranged_int(
            system.get("windows_build"), "system.windows_build", 0, 999999
        ),
        "gpu": gpu,
        "monitor": {
            "width": _ranged_int(monitor.get("width"), "monitor.width", 0, 32768),
            "height": _ranged_int(monitor.get("height"), "monitor.height", 0, 32768),
            "hdr": hdr,
        },
    }


def _settings(raw: dict[str, Any]) -> dict[str, Any]:
    settings = _obj(raw, "settings")
    out: dict[str, Any] = {}
    for channel in ("red", "green", "blue"):
        xy = _obj(settings.get(channel), f"settings.{channel}")
        out[channel] = {
            "x": _ranged_float(xy.get("x"), f"{channel}.x", -2.0, 2.0),
            "y": _ranged_float(xy.get("y"), f"{channel}.y", -2.0, 2.0),
        }
    out["sharpen"] = _ranged_float(settings.get("sharpen"), "settings.sharpen", -1.0, 1.0)
    out["auto_pause"] = _ranged_int(settings.get("auto_pause"), "settings.auto_pause", 0, 3)
    out["game_action"] = _ranged_int(settings.get("game_action"), "settings.game_action", 0, 1)
    out["gpu_priority"] = _ranged_int(
        settings.get("gpu_priority"), "settings.gpu_priority", -7, 7
    )
    return out


def _game(raw: Any) -> dict[str, Any]:
    game = _obj(raw, "games[]")
    how = game.get("how")
    action = game.get("action")
    if how not in _HOWS or action not in _ACTIONS:
        raise _fail("bad_value", "games[].how или action вне списка")
    return {
        "exe": _text(game.get("exe"), "games[].exe", _EXE_RE),
        "how": how,
        "action": action,
        "sessions": _uint(game.get("sessions"), "games[].sessions"),
        "minutes": _uint(game.get("minutes"), "games[].minutes"),
    }


def _stats(raw: dict[str, Any]) -> dict[str, Any]:
    counters = _obj(raw.get("counters"), "counters")
    errors = raw.get("errors")
    games = raw.get("games")
    if not isinstance(errors, list) or not isinstance(games, list):
        raise _fail("bad_type", "errors и games должны быть списками")
    if len(errors) > MAX_ERRORS or len(games) > MAX_GAMES:
        raise _fail("too_many", "слишком много errors или games")
    names = ("uptime_min", "frames_drawn", "frames_echo_skipped", "device_resets", "pauses")
    return {
        "counters": {n: _uint(counters.get(n), f"counters.{n}") for n in names},
        "errors": [_text(e, "errors[]", _PRINTABLE_RE) for e in errors],
        "games": [_game(g) for g in games],
    }


def _comment(value: Any) -> str:
    if not isinstance(value, str):
        raise _fail("bad_type", "comment должен быть строкой")
    if len(value.encode("utf-8")) > MAX_COMMENT_BYTES:
        raise _fail("too_long", "comment длиннее лимита")
    # управляющие символы, кроме перевода строки, в хранилище не нужны
    return "".join(c for c in value if c == "\n" or ord(c) >= 32)


def validate_report(raw: Any) -> dict[str, Any]:
    """Очищенная копия отчёта или ReportError; неизвестные ключи отбрасываются."""
    top = _obj(raw, "отчёт")
    if top.get("schema") != "fringefix-report" or top.get("version") != 1:
        raise _fail("bad_schema", "ожидается fringefix-report версии 1")
    report: dict[str, Any] = {"schema": "fringefix-report", "version": 1, "app": _app(top)}
    if "system" in top:
        report["system"] = _system(top["system"])
    if "settings" in top:
        report["settings"] = _settings(top["settings"])
    # счётчики, ошибки и игры включаются вместе, как один раздел отчёта
    if any(k in top for k in ("counters", "errors", "games")):
        report.update(_stats(top))
    report["comment"] = _comment(top.get("comment", ""))
    return report
