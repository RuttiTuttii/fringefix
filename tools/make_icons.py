"""Генерирует src/res/fringefix.ico и fringefix_off.ico: три полосы R, G, B, зелёная выше.

Только стандартная библиотека. Запуск из корня репозитория: python3 tools/make_icons.py
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

SIZES = (16, 24, 32, 48, 64, 128, 256)
SUPER = 4
COLORS = {
    "on": ((224, 72, 72), (72, 208, 104), (72, 128, 240)),
    "off": ((128, 128, 128),) * 3,
}


def in_bar(x: float, y: float, x0: float, y0: float, w: float, h: float, r: float) -> bool:
    """Точка внутри прямоугольника со скруглёнными углами."""
    if not (x0 <= x <= x0 + w and y0 <= y <= y0 + h):
        return False
    cx = min(max(x, x0 + r), x0 + w - r)
    cy = min(max(y, y0 + r), y0 + h - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def render(size: int, colors: tuple[tuple[int, int, int], ...]) -> bytes:
    """RGBA-картинка с усреднением по SUPER x SUPER подточкам."""
    bar_w, bar_h, gap = size * 0.22, size * 0.58, size * 0.06
    left = (size - (3 * bar_w + 2 * gap)) / 2
    rows = []
    for py in range(size):
        row = bytearray([0])
        for px in range(size):
            acc = [0, 0, 0, 0]
            for sy in range(SUPER):
                for sx in range(SUPER):
                    x = px + (sx + 0.5) / SUPER
                    y = py + (sy + 0.5) / SUPER
                    for i, color in enumerate(colors):
                        top = size * (0.12 if i == 1 else 0.30)
                        if in_bar(x, y, left + i * (bar_w + gap), top, bar_w, bar_h, bar_w * 0.3):
                            acc[0] += color[0]
                            acc[1] += color[1]
                            acc[2] += color[2]
                            acc[3] += 255
                            break
            n = SUPER * SUPER
            alpha = acc[3] // n
            # цвет считаем по покрытым подточкам, чтобы край не темнел от прозрачного чёрного
            covered = max(acc[3] // 255, 1)
            row += bytes([acc[0] // covered, acc[1] // covered, acc[2] // covered, alpha])
        rows.append(bytes(row))
    return b"".join(rows)


def png(size: int, raw: bytes) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def ico(colors: tuple[tuple[int, int, int], ...]) -> bytes:
    images = [png(s, render(s, colors)) for s in SIZES]
    head = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    for size, data in zip(SIZES, images):
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    return head + entries + b"".join(images)


def main() -> None:
    out = Path(__file__).resolve().parent.parent / "src" / "res"
    out.mkdir(parents=True, exist_ok=True)
    (out / "fringefix.ico").write_bytes(ico(COLORS["on"]))
    (out / "fringefix_off.ico").write_bytes(ico(COLORS["off"]))


if __name__ == "__main__":
    main()
