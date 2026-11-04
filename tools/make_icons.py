"""Собирает ICO для ресурсов exe из SVG в assets/: значок приложения и флаги языков.

Значок выключенного оверлея не рисуется отдельно: это те же SVG под монохромным фильтром.

Нужен rsvg-convert (librsvg). Запуск из корня репозитория: python3 tools/make_icons.py
Результат лежит в репозитории готовым, пересобирать нужно только при правке SVG.
"""
from __future__ import annotations

import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
OUT = ROOT / "src" / "res"
APP_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
FLAG_SIZES = (16, 20, 24, 32)
# до этого размера берётся знак без плитки: на мелких размерах плитка съедает форму
MARK_MAX = 32


# выключенный оверлей: тот же рисунок под монохромным фильтром, цвета уходят, яркость шариков остаётся
OFF_FILTER = (
    '<defs><filter id="ff-off" color-interpolation-filters="sRGB">'
    '<feColorMatrix type="saturate" values="0"/></filter></defs>'
    '<g filter="url(#ff-off)">'
)


def with_off_filter(svg_text: str) -> str:
    """Оборачивает содержимое SVG в группу с фильтром выключенного состояния."""
    start = svg_text.index(">", svg_text.index("<svg")) + 1
    end = svg_text.rindex("</svg>")
    return svg_text[:start] + OFF_FILTER + svg_text[start:end] + "</g>" + svg_text[end:]


def render(svg: Path, size: int, off: bool = False) -> bytes:
    """PNG нужного размера из SVG через rsvg-convert; off накладывает монохромный фильтр выключенного состояния."""
    text = svg.read_text(encoding="utf-8")
    if off:
        text = with_off_filter(text)
    result = subprocess.run(
        ["rsvg-convert", "-w", str(size), "-h", str(size), "--keep-aspect-ratio"],
        input=text.encode("utf-8"),
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"rsvg-convert не смог собрать {svg.name}: {result.stderr.decode()}")
    return result.stdout


def pack_ico(images: list[tuple[int, bytes]]) -> bytes:
    """ICO из PNG-картинок: заголовок, каталог и сами PNG."""
    head = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    for size, data in images:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    return head + entries + b"".join(data for _, data in images)


def app_icon(off: bool) -> bytes:
    images = [
        (s, render(ASSETS / ("mark.svg" if s <= MARK_MAX else "icon.svg"), s, off))
        for s in APP_SIZES
    ]
    return pack_ico(images)


def flag_icon(name: str) -> bytes:
    return pack_ico([(s, render(ASSETS / "flags" / f"{name}.svg", s)) for s in FLAG_SIZES])


def main() -> int:
    if shutil.which("rsvg-convert") is None:
        print("нужен rsvg-convert: pacman -S librsvg или apt install librsvg2-bin", file=sys.stderr)
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "fringefix.ico").write_bytes(app_icon(off=False))
    (OUT / "fringefix_off.ico").write_bytes(app_icon(off=True))
    for name in ("ru", "en", "auto"):
        (OUT / f"flag_{name}.ico").write_bytes(flag_icon(name))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
