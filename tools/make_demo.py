"""Рисует assets/readme/fringe-demo-ru.svg и -en.svg: симуляция текста на QD-OLED до и после коррекции.

Настоящий шрифт растеризуется ImageMagick в серую картинку, затем каждый её пиксель разбирается на три
субпикселя треугольной раскладки: красный и синий снизу от центра на 0.25 влево и вправо, зелёный сверху
на 0.25. Слева панель, как её видит глаз без коррекции, справа с коррекцией: каждый субпиксель показывает
картинку там, где он стоит физически. Это симуляция, а не фотография.

Нужен ImageMagick (magick). Запуск из корня репозитория: python3 tools/make_demo.py
"""
from __future__ import annotations

import base64
import math
import struct
import subprocess
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "assets" / "readme"
FONT = "/usr/share/fonts/noto/NotoSans-Regular.ttf"
WORD = "fringefix"
POINT = 17
WIDTH, HEIGHT = 84, 26
EYE = 6      # пикселей рисунка на пиксель экрана в виде глазом
LENS = 18    # то же для увеличенного вида субпикселей
CROP = (7, 5, 35, 19)  # левая часть слова под лупой: x0, y0, x1, y1
SIGMA = 0.5
GAMMA = 1 / 1.7
# смещения субпикселей от центра пикселя в долях пикселя, y вниз: красный, зелёный, синий
OFFSETS = {"r": (-0.25, 0.25), "g": (0.0, -0.25), "b": (0.25, 0.25)}
COLORS = {"r": "#ff2d2d", "g": "#2dff5a", "b": "#3d6bff"}
SUB = 0.44

TEXT = {
    "ru": ("Без коррекции", "С fringefix", "Как видит глаз", "Под лупой: субпиксели",
           "Симуляция по модели субпикселей QD-OLED с треугольной раскладкой, не фотография"),
    "en": ("Without correction", "With fringefix", "What the eye sees", "Under a magnifier: subpixels",
           "A simulation of the QD-OLED triangular subpixel layout, not a photo"),
}


def source() -> list[list[float]]:
    """Слово настоящим шрифтом: белое на чёрном, серые уровни сглаживания, как у текста на экране."""
    raw = subprocess.run(
        ["magick", "-size", f"{WIDTH}x{HEIGHT}", "xc:black", "-font", FONT, "-pointsize", str(POINT),
         "-fill", "white", "-gravity", "center", "-annotate", "+0+0", WORD,
         "-colorspace", "Gray", "-depth", "8", "pgm:-"],
        capture_output=True, check=True,
    ).stdout
    # формат P5: заголовок из трёх строк, затем байты
    header_end = 0
    for _ in range(3):
        header_end = raw.index(b"\n", header_end) + 1
    pixels = raw[header_end:]
    return [[pixels[y * WIDTH + x] / 255 for x in range(WIDTH)] for y in range(HEIGHT)]


def sample(img: list[list[float]], x: float, y: float) -> float:
    """Билинейная выборка в координатах, где центр пикселя (i, j) это (i + 0.5, j + 0.5)."""
    h, w = len(img), len(img[0])
    tx, ty = x - 0.5, y - 0.5
    i, j = int(tx // 1), int(ty // 1)
    fx, fy = tx - i, ty - j

    def px(a: int, b: int) -> float:
        return img[b][a] if 0 <= a < w and 0 <= b < h else 0.0

    top = px(i, j) * (1 - fx) + px(i + 1, j) * fx
    bottom = px(i, j + 1) * (1 - fx) + px(i + 1, j + 1) * fx
    return top * (1 - fy) + bottom * fy


def channel_value(img: list[list[float]], i: int, j: int, ch: str, corrected: bool) -> float:
    """Что показывает субпиксель ch пикселя (i, j): без коррекции значение пикселя, с коррекцией картинку в его точке."""
    dx, dy = OFFSETS[ch]
    return sample(img, i + 0.5 + dx, j + 0.5 + dy) if corrected else img[j][i]


def png_bytes(width: int, height: int, rows: list[bytes]) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))


def perceived_png(img: list[list[float]], corrected: bool) -> bytes:
    """Вид глазом: каждый субпиксель размывается гауссианом, каналы складываются, затем гамма."""
    h, w = len(img), len(img[0])
    ow, oh = w * EYE, h * EYE
    chans = {c: [0.0] * (ow * oh) for c in "rgb"}
    reach = int(3 * SIGMA * EYE)
    norm = 1 / (2 * math.pi * SIGMA**2)
    for j in range(h):
        for i in range(w):
            for ch, (dx, dy) in OFFSETS.items():
                v = channel_value(img, i, j, ch, corrected)
                if v < 0.01:
                    continue
                cx, cy = (i + 0.5 + dx) * EYE, (j + 0.5 + dy) * EYE
                for y in range(max(0, int(cy) - reach), min(oh, int(cy) + reach + 1)):
                    for x in range(max(0, int(cx) - reach), min(ow, int(cx) + reach + 1)):
                        d2 = ((x + 0.5 - cx) / EYE) ** 2 + ((y + 0.5 - cy) / EYE) ** 2
                        chans[ch][y * ow + x] += v * norm * math.exp(-d2 / (2 * SIGMA**2))
    rows = []
    for y in range(oh):
        row = bytearray([0])
        for x in range(ow):
            for c in "rgb":
                row.append(int(255 * min(1.0, chans[c][y * ow + x] / 1.0) ** GAMMA))
        rows.append(bytes(row))
    return png_bytes(ow, oh, rows)


def lens_panel(img: list[list[float]], corrected: bool, ox: float, oy: float) -> str:
    """Субпиксели участка CROP квадратиками, как под лупой."""
    x0, y0, x1, y1 = CROP
    parts = [f'<rect x="{ox}" y="{oy}" width="{(x1 - x0) * LENS}" height="{(y1 - y0) * LENS}" fill="#000"/>']
    for j in range(y0, y1):
        for i in range(x0, x1):
            for ch, (dx, dy) in OFFSETS.items():
                v = channel_value(img, i, j, ch, corrected)
                if v < 0.02:
                    continue
                cx = ox + (i - x0 + 0.5 + dx) * LENS
                cy = oy + (j - y0 + 0.5 + dy) * LENS
                s = SUB * LENS
                parts.append(f'<rect x="{cx - s / 2:.1f}" y="{cy - s / 2:.1f}" width="{s:.1f}" height="{s:.1f}" '
                             f'rx="1.6" fill="{COLORS[ch]}" fill-opacity="{v:.2f}"/>')
    return "\n".join(parts)


def data_uri(png: bytes) -> str:
    return "data:image/png;base64," + base64.b64encode(png).decode("ascii")


def build(lang: str) -> str:
    img = source()
    pw = WIDTH * EYE
    eye_h = HEIGHT * EYE
    lens_h = (CROP[3] - CROP[1]) * LENS
    before, after, eye, lens, note = TEXT[lang]
    gap, margin, head, sub = 28, 24, 44, 26
    width = pw * 2 + gap + margin * 2
    row1 = head + sub
    row2 = row1 + eye_h + 16 + sub
    height = row2 + lens_h + 56
    lx, rx = margin, margin + pw + gap
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}" role="img" aria-labelledby="t">
  <title id="t">{before} / {after}</title>
  <style>
    .bg {{ fill: #101522; }}
    .lbl {{ fill: #e8ecf5; font: 600 17px system-ui, 'Segoe UI', sans-serif; }}
    .sub {{ fill: #98a2b8; font: 13px system-ui, 'Segoe UI', sans-serif; }}
    @media (prefers-color-scheme: light) {{
      .bg {{ fill: #eef1f8; }}
      .lbl {{ fill: #1a1f2e; }}
      .sub {{ fill: #566079; }}
    }}
  </style>
  <rect class="bg" width="{width}" height="{height}" rx="14"/>
  <text class="lbl" x="{lx}" y="32">{before}</text>
  <text class="lbl" x="{rx}" y="32">{after}</text>
  <text class="sub" x="{lx}" y="{row1 - 10}">{eye}</text>
  <text class="sub" x="{rx}" y="{row1 - 10}">{eye}</text>
  <image x="{lx}" y="{row1}" width="{pw}" height="{eye_h}" href="{data_uri(perceived_png(img, False))}"/>
  <image x="{rx}" y="{row1}" width="{pw}" height="{eye_h}" href="{data_uri(perceived_png(img, True))}"/>
  <text class="sub" x="{lx}" y="{row2 - 10}">{lens}</text>
  <text class="sub" x="{rx}" y="{row2 - 10}">{lens}</text>
  <g>
{lens_panel(img, False, lx, row2)}
  </g>
  <g>
{lens_panel(img, True, rx, row2)}
  </g>
  <text class="sub" x="{margin}" y="{height - 18}">{note}</text>
</svg>
"""


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for lang in ("ru", "en"):
        (OUT / f"fringe-demo-{lang}.svg").write_text(build(lang), encoding="utf-8")


if __name__ == "__main__":
    main()
