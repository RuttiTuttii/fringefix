"""Рисует окна fringefix в assets/screens/ как SVG для README: четыре окна, два языка, две темы.

Размеры и подписи берутся из тех же таблиц, что и в коде программы
(include/ui_layout.h повторён здесь константами, строки читаются из src/i18n.c), поэтому состав и
расположение элементов совпадают с настоящими окнами.

Запуск из корня репозитория: python3 tools/make_screens.py
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "screens"
FONT = "'Segoe UI Variable', 'Segoe UI', system-ui, sans-serif"
MONO = "Consolas, 'Cascadia Mono', ui-monospace, monospace"
ACCENT = "#0078d4"

# константы окна настроек из include/ui_layout.h
MARGIN, CARD_W, CARD_HEAD, ROW_H, CARD_PAD, CARD_GAP = 16, 488, 26, 30, 10, 8
MENU_H = 26
TOP = MENU_H + 32
COL_LABEL, COL_SLIDER, SLIDER_W, COL_EDIT, EDIT_W, COL_UNIT = 14, 150, 236, 396, 58, 460
BTN_H = 30


@dataclass(frozen=True)
class Palette:
    bg: str
    card: str
    field: str
    fg: str
    muted: str
    border: str
    window: str


DARK = Palette("#202020", "#2b2b2b", "#333333", "#ffffff", "#a0a0a0", "#424242", "#202020")
LIGHT = Palette("#f3f3f3", "#ffffff", "#ffffff", "#1a1a1a", "#606060", "#e0e0e0", "#f3f3f3")


def load_strings(lang: str) -> dict[str, str]:
    """Тексты интерфейса из src/i18n.c: таблица RU или EN, склейка соседних литералов L"..." L"..."."""
    src = (ROOT / "src" / "i18n.c").read_text(encoding="utf-8")
    name = "RU" if lang == "ru" else "EN"
    start = src.index(f"static const wchar_t *const {name}[S_COUNT]")
    end = src.index("};", start)
    out: dict[str, str] = {}
    for m in re.finditer(r'\[S_(\w+)\]\s*=\s*((?:L"(?:[^"\\]|\\.)*"\s*)+),', src[start:end]):
        parts = re.findall(r'L"((?:[^"\\]|\\.)*)"', m.group(2))
        text = "".join(parts).replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"').replace("\\\\", "\\")
        out[m.group(1)] = text
    return out


def mix(a: str, b: str, t: int) -> str:
    """Смесь двух цветов #rrggbb: t из 256 даёт долю b."""
    ca = [int(a[i:i + 2], 16) for i in (1, 3, 5)]
    cb = [int(b[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{(x * (256 - t) + y * t) // 256:02x}" for x, y in zip(ca, cb))


def esc(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def wrap(text: str, width: int) -> list[str]:
    lines: list[str] = []
    for para in text.split("\n"):
        line = ""
        for word in para.split(" "):
            if len(line) + len(word) + 1 > width and line:
                lines.append(line)
                line = word
            else:
                line = f"{line} {word}".strip()
        lines.append(line)
    return lines


class Canvas:
    """Накопитель SVG-элементов с примитивами окна."""

    def __init__(self, pal: Palette) -> None:
        self.p = pal
        self.parts: list[str] = []

    def add(self, s: str) -> None:
        self.parts.append(s)

    def text(self, x: float, y: float, s: str, fill: str | None = None, weight: int = 400,
             size: int = 12, anchor: str = "start", mono: bool = False) -> None:
        family = MONO if mono else FONT
        self.add(f'<text x="{x}" y="{y}" fill="{fill or self.p.fg}" font-family="{family}" '
                 f'font-size="{size}" font-weight="{weight}" text-anchor="{anchor}">{esc(s)}</text>')

    def paragraph(self, x: float, y: float, s: str, width: int, fill: str | None = None, lead: int = 17) -> float:
        for i, line in enumerate(wrap(s, width)):
            self.text(x, y + i * lead, line, fill)
        return y + len(wrap(s, width)) * lead

    def card(self, x: float, y: float, w: float, h: float, title: str) -> None:
        self.add(f'<rect x="{x + .5}" y="{y + .5}" width="{w - 1}" height="{h - 1}" rx="8" '
                 f'fill="{self.p.card}" stroke="{self.p.border}"/>')
        self.text(x + COL_LABEL, y + 18, title, ACCENT, 600, 13)

    def card_accent(self, x: float, y: float, w: float, h: float, title: str, badge: str) -> None:
        """Карточка отдельной возможности, как paint_card_accent: оттенок акцента, полоса слева, плашка у заголовка."""
        fill = mix(self.p.card, ACCENT, 34 if self.p.window == "#202020" else 22)
        edge = mix(self.p.border, ACCENT, 140)
        self.add(f'<rect x="{x + .5}" y="{y + .5}" width="{w - 1}" height="{h - 1}" rx="8" fill="{fill}" stroke="{edge}"/>')
        self.add(f'<rect x="{x + 1}" y="{y + 10}" width="4" height="{h - 20}" fill="{ACCENT}"/>')
        self.text(x + COL_LABEL, y + 18, title, ACCENT, 600, 13)
        bx = x + COL_LABEL + len(title) * 7.4 + 10
        self.add(f'<rect x="{bx}" y="{y + 6}" width="{len(badge) * 7 + 14}" height="16" rx="8" fill="{ACCENT}"/>')
        self.text(bx + (len(badge) * 7 + 14) / 2, y + 18, badge, "#ffffff", 600, 11, "middle")

    def menubar(self, width: int, titles: list[str]) -> None:
        """Полоса меню окна настроек, как menubar_paint: цвет карточки, линия снизу, заголовки подряд."""
        self.add(f'<rect width="{width}" height="{MENU_H}" fill="{self.p.card}"/>')
        self.add(f'<rect y="{MENU_H - 1}" width="{width}" height="1" fill="{self.p.border}"/>')
        x = MARGIN - 4
        for title in titles:
            w = len(title) * 7 + 24
            self.text(x + w / 2, 17, title, anchor="middle")
            x += w

    def slider(self, x: float, y: float, w: float, pos: float, lo: float, hi: float) -> None:
        """Ползунок как в slider.c: дорожка, заливка от нуля, отметка нуля, круг с кольцом."""
        pad, cy = 11, y + 14
        span = w - 2 * pad
        px = x + pad + (pos - lo) / (hi - lo) * span
        zx = x + pad + (0 - lo) / (hi - lo) * span if lo < 0 < hi else x + pad
        self.add(f'<rect x="{x + pad}" y="{cy - 2.5}" width="{span}" height="5" rx="2.5" fill="{self.p.border}"/>')
        self.add(f'<rect x="{min(px, zx)}" y="{cy - 2.5}" width="{abs(px - zx)}" height="5" rx="2.5" fill="{ACCENT}"/>')
        if lo < 0 < hi:
            self.add(f'<rect x="{zx - .5}" y="{cy - 9}" width="1" height="18" fill="{self.p.muted}"/>')
        self.add(f'<circle cx="{px}" cy="{cy}" r="8" fill="{ACCENT}" stroke="{self.p.card}" stroke-width="2"/>')

    def edit(self, x: float, y: float, w: float, h: float, s: str) -> None:
        self.add(f'<rect x="{x + .5}" y="{y + .5}" width="{w - 1}" height="{h - 1}" rx="3" fill="{self.p.field}" stroke="{self.p.border}"/>')
        self.text(x + w - 7, y + h / 2 + 4, s, anchor="end")

    def combo(self, x: float, y: float, w: float, s: str, flag: str | None = None, locked: bool = False) -> None:
        self.add(f'<rect x="{x + .5}" y="{y + .5}" width="{w - 1}" height="23" rx="3" fill="{self.p.field}" stroke="{self.p.border}"/>')
        tx = x + 8
        if flag:
            self.add(flag_svg(flag, x + 8, y + 4))
            tx = x + 32
        self.text(tx, y + 16, s, self.p.muted if locked else None)
        self.add(f'<path d="M{x + w - 18} {y + 9}l4 4 4-4" fill="none" stroke="{self.p.muted}" stroke-width="1.5"/>')

    def switch(self, x: float, y: float, on: bool) -> None:
        """Выключатель как в slider.c с диапазоном из двух положений: заливка и круг у правого края, когда включён."""
        pad, cy, w = 11, y + 14, 60
        self.add(f'<rect x="{x + pad}" y="{cy - 2.5}" width="{w - 2 * pad}" height="5" rx="2.5" fill="{self.p.border}"/>')
        px = x + w - pad if on else x + pad
        if on:
            self.add(f'<rect x="{x + pad}" y="{cy - 2.5}" width="{w - 2 * pad}" height="5" rx="2.5" fill="{ACCENT}"/>')
        self.add(f'<circle cx="{px}" cy="{cy}" r="8" fill="{ACCENT if on else self.p.muted}" stroke="{self.p.card}" stroke-width="2"/>')

    def button(self, x: float, y: float, w: float, s: str) -> None:
        self.add(f'<rect x="{x + .5}" y="{y + .5}" width="{w - 1}" height="{BTN_H - 1}" rx="4" fill="{self.p.field}" stroke="{self.p.border}"/>')
        self.text(x + w / 2, y + BTN_H / 2 + 4, s, anchor="middle")

    def checkbox(self, x: float, y: float, s: str, on: bool = True) -> None:
        self.add(f'<rect x="{x + .5}" y="{y + .5}" width="15" height="15" rx="3" fill="{ACCENT if on else self.p.field}" stroke="{ACCENT if on else self.p.muted}"/>')
        if on:
            self.add(f'<path d="M{x + 4} {y + 8.5}l3 3 5-6" fill="none" stroke="#fff" stroke-width="1.8"/>')
        self.text(x + 24, y + 12.5, s)


def flag_svg(kind: str, x: float, y: float) -> str:
    """Флаг 16 на 12 из простых фигур, как значок в списке языков."""
    if kind == "ru":
        body = ('<rect width="16" height="4" fill="#fff"/><rect y="4" width="16" height="4" fill="#1c57a5"/>'
                '<rect y="8" width="16" height="4" fill="#d52b1e"/>')
    elif kind == "en":
        body = ('<rect width="16" height="12" fill="#012169"/>'
                '<path d="M0 0L16 12M16 0L0 12" stroke="#fff" stroke-width="2.4"/>'
                '<path d="M0 0L16 12M16 0L0 12" stroke="#c8102e" stroke-width="1"/>'
                '<path d="M8 0V12M0 6H16" stroke="#fff" stroke-width="4"/><path d="M8 0V12M0 6H16" stroke="#c8102e" stroke-width="2.4"/>')
    else:
        body = ('<circle cx="8" cy="6" r="6" fill="#3b82f6"/><ellipse cx="8" cy="6" rx="2.6" ry="6" fill="none" stroke="#fff" stroke-width=".9"/>'
                '<path d="M2 6H14" stroke="#fff" stroke-width=".9"/>')
    return f'<g transform="translate({x} {y})">{body}</g>'


def window(c: Canvas, title: str, w: int, h: int) -> str:
    """Рамка окна Windows 11: полоса заголовка со значком и кнопками, тонкая граница."""
    p = c.p
    chrome = [
        f'<rect x=".5" y=".5" width="{w - 1}" height="{h + 31}" rx="9" fill="{p.window}" stroke="{p.border}"/>',
        '<g transform="translate(10 8)"><circle cx="5" cy="5" r="3.6" fill="#ef4444"/><circle cx="13" cy="5" r="3.6" fill="#3b82f6"/><circle cx="9" cy="12" r="3.6" fill="#22c55e"/></g>',
        f'<text x="34" y="21" fill="{p.fg}" font-family="{FONT}" font-size="12">{esc(title)}</text>',
        f'<path d="M{w - 126} 16h10M{w - 80} 11h9v9h-9z" fill="none" stroke="{p.fg}" stroke-width="1"/>',
        f'<path d="M{w - 34} 11l9 9m0-9l-9 9" stroke="{p.fg}" stroke-width="1.1"/>',
    ]
    body = "".join(c.parts)
    return "".join(chrome) + f'<g transform="translate(0 32)">{body}</g>'


def svg(inner: str, w: int, h: int, title: str) -> str:
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h + 32}" width="{w}" height="{h + 32}" role="img" aria-label="{esc(title)}">'
            f'<title>{esc(title)}</title>{inner}</svg>\n')


# ---------- окна
def settings(pal: Palette, t: dict[str, str]) -> str:
    c = Canvas(pal)
    height = 614
    c.add(f'<rect width="520" height="{height}" fill="{pal.bg}"/>')
    c.menubar(520, [t["M_FILE"], t["M_EDIT"], t["M_TOOLS"], t["M_HELP"]])
    c.text(MARGIN, MENU_H + 22, t["HINT"], pal.muted, size=11)
    # карточка выбранного монитора: монитор, выключатель его фильтра, резкость
    y = TOP
    h = CARD_HEAD + 3 * ROW_H + CARD_PAD
    c.card(MARGIN, y, CARD_W, h, t["IMAGE"])
    c.text(MARGIN + COL_LABEL, y + CARD_HEAD + 19, t["MONITOR"])
    c.combo(MARGIN + COL_SLIDER, y + CARD_HEAD + 2, CARD_W - COL_SLIDER - COL_LABEL,
            "0: \\\\.\\DISPLAY1 (3840x2160)  [" + t["MON_TAG_ON"] + "]")
    c.text(MARGIN + COL_LABEL, y + CARD_HEAD + ROW_H + 19, t["FILTER"])
    c.switch(MARGIN + COL_SLIDER, y + CARD_HEAD + ROW_H + 1, True)
    c.text(MARGIN + COL_SLIDER + 72, y + CARD_HEAD + ROW_H + 19, t["STATE_ON"])
    c.text(MARGIN + COL_LABEL, y + CARD_HEAD + 2 * ROW_H + 19, t["SHARPEN"])
    c.slider(MARGIN + COL_SLIDER, y + CARD_HEAD + 2 * ROW_H + 1, SLIDER_W, 0.0, -1, 1)
    c.edit(MARGIN + COL_EDIT, y + CARD_HEAD + 2 * ROW_H + 3, EDIT_W, 24, "0.00")
    y += h + CARD_GAP
    cards = [("RED", (0.25, -0.25)), ("GREEN", (0.0, 0.25)), ("BLUE", (-0.25, -0.25))]
    for name, (x_val, y_val) in cards:
        h = CARD_HEAD + 2 * ROW_H + CARD_PAD
        c.card(MARGIN, y, CARD_W, h, t[name])
        for row, (key, val) in enumerate((("HORIZONTAL", x_val), ("VERTICAL", y_val))):
            ry = y + CARD_HEAD + row * ROW_H
            c.text(MARGIN + COL_LABEL, ry + 19, t[key])
            c.slider(MARGIN + COL_SLIDER, ry + 1, SLIDER_W, val, -2, 2)
            c.edit(MARGIN + COL_EDIT, ry + 3, EDIT_W, 24, f"{val:.2f}")
            c.text(MARGIN + COL_UNIT, ry + 19, "px", pal.muted)
        y += h + CARD_GAP
    bottom = y - CARD_GAP
    c.text(MARGIN, bottom + 24, t["VERSION"].replace("%ls", "v0.8.0-beta.1"), pal.muted)
    bw = (CARD_W - 32) // 3
    rows = [("BTN_PATTERN", "BTN_RESET", "BTN_PROGRAM"), ("BTN_PROFILE_LOAD", "BTN_PROFILE_SAVE", "BTN_SAVE")]
    for r, names in enumerate(rows):
        for col, key in enumerate(names):
            c.button(MARGIN + col * (bw + 16), bottom + 40 + r * 34, bw, t[key])
    return svg(window(c, t["WINDOW_TITLE"], 520, height), 520, height, t["WINDOW_TITLE"])


def program(pal: Palette, t: dict[str, str]) -> str:
    c = Canvas(pal)
    height = 766
    c.add(f'<rect width="520" height="{height}" fill="{pal.bg}"/>')
    langname = t["LANG_RU"] if t["LANGUAGE"] == "Язык" else t["LANG_EN"]
    flag = "ru" if t["LANGUAGE"] == "Язык" else "en"
    combo_w = CARD_W - COL_SLIDER - COL_LABEL
    inner = CARD_W - 2 * COL_LABEL
    # карточки как в ui_app.c: интерфейс, автопауза, ReShade, сочетания, автозапуск, данные
    y = 16
    c.card(MARGIN, y, CARD_W, 96, t["CARD_INTERFACE"])
    c.text(MARGIN + COL_LABEL, y + 26 + 19, t["LANGUAGE"])
    c.combo(MARGIN + COL_SLIDER, y + 26 + 1, combo_w, langname, flag)
    c.text(MARGIN + COL_LABEL, y + 56 + 19, t["THEME"])
    c.combo(MARGIN + COL_SLIDER, y + 56 + 1, combo_w, t["THEME_AUTO"])
    y += 96 + CARD_GAP
    c.card(MARGIN, y, CARD_W, 96, t["CARD_AUTOPAUSE"])
    c.text(MARGIN + COL_LABEL, y + 26 + 19, t["AUTOPAUSE"])
    c.combo(MARGIN + COL_SLIDER, y + 26 + 1, combo_w, t["AP_3D"])
    c.button(MARGIN + COL_LABEL, y + 56 + 1, inner, t["BTN_APP_LISTS"])
    y += 96 + CARD_GAP
    c.card_accent(MARGIN, y, CARD_W, 126, t["CARD_RESHADE"], "beta")
    c.paragraph(MARGIN + COL_LABEL + 6, y + 26 + 15, t["RS_NOTE"], 74, pal.muted, 15)
    small = 130
    c.button(MARGIN + COL_LABEL, y + 86 + 1, small, t["BTN_RS_INSTALL"])
    c.button(MARGIN + COL_LABEL + small + 8, y + 86 + 1, small, t["BTN_RS_REMOVE"])
    c.button(MARGIN + COL_LABEL + 2 * (small + 8), y + 86 + 1, inner - 2 * small - 16, t["BTN_RS_EXPORT"])
    y += 126 + CARD_GAP
    c.card(MARGIN, y, CARD_W, 96, t["CARD_HOTKEYS"])
    for row, (key, value) in enumerate((("HK_TOGGLE", "F10"), ("HK_QUIT", "F11"))):
        c.text(MARGIN + COL_LABEL, y + 26 + row * 30 + 19, t[key])
        c.edit(MARGIN + COL_SLIDER, y + 26 + row * 30 + 2, 160, 24, value)
    y += 96 + CARD_GAP
    c.card(MARGIN, y, CARD_W, 66, t["CARD_AUTOSTART"])
    c.checkbox(MARGIN + COL_LABEL, y + 26 + 4, t["AUTOSTART_CHECK"], on=False)
    y += 66 + CARD_GAP
    c.card(MARGIN, y, CARD_W, 162, t["CARD_DATA"])
    c.checkbox(MARGIN + COL_LABEL, y + 26 + 4, t["CHECK_UPDATES"])
    c.checkbox(MARGIN + COL_LABEL, y + 56 + 4, t["BETA_CHECK"], on=False)
    bw = (inner - 16) // 3
    for i, key in enumerate(("MENU_UPDATES", "BTN_VERSIONS", "BTN_REPORT")):
        c.button(MARGIN + COL_LABEL + i * (bw + 8), y + 86 + 1, bw, t[key])
    c.button(MARGIN + COL_LABEL, y + 116 + 1 + 6, inner, t["BTN_DIAG"])
    y += 162
    c.button(MARGIN + CARD_W - 120, y + 16, 120, t["VER_CLOSE"])
    return svg(window(c, t["APP_TITLE"], 520, height), 520, height, t["APP_TITLE"])


def report(pal: Palette, t: dict[str, str]) -> str:
    c = Canvas(pal)
    c.add(f'<rect width="640" height="702" fill="{pal.bg}"/>')
    c.paragraph(24, 28, t["REP_HINT"], 106, pal.muted, 16)
    for i, key in enumerate(("REP_COLLECT", "REP_SYSTEM", "REP_SETTINGS", "REP_STATS")):
        c.checkbox(24, 86 + i * 22, t[key], on=(i != 0))
    c.text(24, 188, t["REP_COMMENT"])
    c.edit(24, 194, 592, 72, "")
    c.text(34, 214, "Спасибо за приложение!" if t["REP_SEND"] == "Отправить" else "Thanks for the app!")
    c.text(24, 288, t["REP_PREVIEW"])
    c.add(f'<rect x="24.5" y="296.5" width="591" height="281" rx="3" fill="{pal.field}" stroke="{pal.border}"/>')
    fixture = json.loads((ROOT / "server" / "tests" / "fixtures" / "report_v1.json").read_text(encoding="utf-8"))
    fixture["comment"] = "Спасибо за приложение!" if t["REP_SEND"] == "Отправить" else "Thanks for the app!"
    lines = json.dumps(fixture, ensure_ascii=False, indent=2).split("\n")[:19]
    for i, line in enumerate(lines):
        c.text(34, 314 + i * 14.2, line, pal.fg if i % 2 == 0 else pal.fg, size=11, mono=True)
    bw = (640 - 48 - 24) // 3
    for r, keys in enumerate((("REP_SEND", "REP_ISSUE", "REP_COPY"), ("REP_SAVE", "REP_CLEAR", "VER_CLOSE"))):
        for col, key in enumerate(keys):
            c.button(24 + col * (bw + 12), 614 + r * 38, bw, t[key])
    return svg(window(c, t["REP_TITLE"], 640, 702), 640, 702, t["REP_TITLE"])


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    builders = {"settings": settings, "program": program, "report": report}
    for lang in ("ru", "en"):
        strings = load_strings(lang)
        for theme, pal in (("dark", DARK), ("light", LIGHT)):
            for name, build in builders.items():
                (OUT / f"{name}-{lang}-{theme}.svg").write_text(build(pal, strings), encoding="utf-8")


if __name__ == "__main__":
    main()
