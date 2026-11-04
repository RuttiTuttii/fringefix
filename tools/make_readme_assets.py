"""Рисует остальные SVG для README: баннер, значки возможностей и схему работы.

Всё плоское, без градиентов и свечения; нейтральные цвета меняются по prefers-color-scheme.
Запуск из корня репозитория: python3 tools/make_readme_assets.py
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
README = ROOT / "assets" / "readme"
ICONS = ROOT / "assets" / "icons"

STYLE = """<style>
    .ink { fill: none; stroke: #94a3b8; stroke-width: 3; stroke-linecap: round; stroke-linejoin: round; }
    .fill { fill: #94a3b8; }
    .bg { fill: #131a2a; }
    .t1 { fill: #f1f4fb; }
    .t2 { fill: #9aa5bd; }
    .box { fill: #1c2538; stroke: #31405e; }
    .chip { fill: #1c2538; stroke: #31405e; }
    .arrow { fill: none; stroke: #6b7a99; stroke-width: 2.5; }
    @media (prefers-color-scheme: light) {
      .ink { stroke: #475569; }
      .fill { fill: #475569; }
      .bg { fill: #eef1f8; }
      .t1 { fill: #131a2a; }
      .t2 { fill: #566079; }
      .box { fill: #ffffff; stroke: #cfd7e6; }
      .chip { fill: #ffffff; stroke: #cfd7e6; }
      .arrow { stroke: #7a88a6; }
    }
  </style>"""
R, G, B = "#ef4444", "#22c55e", "#3b82f6"
FONT = "system-ui, 'Segoe UI', sans-serif"


def icon(name: str, body: str) -> None:
    ICONS.mkdir(parents=True, exist_ok=True)
    (ICONS / f"{name}.svg").write_text(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64" role="img" aria-label="{name}">\n'
        f"  {STYLE}\n  {body}\n</svg>\n", encoding="utf-8")


def star(cx: float, cy: float, r: float, color: str) -> str:
    """Четырёхлучевая искра."""
    k = r * 0.28
    return (f'<path d="M{cx} {cy - r}L{cx + k} {cy - k}L{cx + r} {cy}L{cx + k} {cy + k}L{cx} {cy + r}'
            f'L{cx - k} {cy + k}L{cx - r} {cy}L{cx - k} {cy - k}Z" fill="{color}"/>')


def icons() -> None:
    # сдвиг каналов: пучок шариков и две стрелки
    icon("shift", f'<circle cx="32" cy="38" r="10" fill="{G}"/><circle cx="19" cy="17" r="10" fill="{R}"/>'
                  f'<circle cx="45" cy="17" r="10" fill="{B}"/><path class="ink" d="M10 56h14M20 51l-5 5 5 5M54 56H40M44 51l5 5-5 5"/>')
    # мастер: палочка с синим кончиком и три искры вокруг
    icon("wizard", f'<path class="ink" d="M11 53L33 31"/><path d="M33 31l8-8" stroke="{B}" stroke-width="7" stroke-linecap="round"/>'
                   f'{star(53, 14, 9, G)}{star(56, 37, 6, R)}{star(27, 12, 7, B)}')
    # игры: геймпад с крестовиной и двумя кнопками
    icon("games", f'<rect class="ink" x="6" y="17" width="52" height="30" rx="15"/><path class="ink" d="M20 26v12M14 32h12"/>'
                  f'<circle cx="43" cy="28" r="3.6" fill="{R}"/><circle cx="50" cy="35" r="3.6" fill="{B}"/>')
    # профили: две карточки, у задней видны только верх и левый край, на передней фигурные скобки
    icon("profiles", '<path class="ink" d="M11 41V15q0-4 4-4h26q4 0 4 4v4"/><rect class="ink" x="19" y="19" width="34" height="34" rx="6"/>'
                     f'<path d="M33 27q-4 0-4 4v2q0 3-3 3 3 0 3 3v2q0 4 4 4M39 27q4 0 4 4v2q0 3 3 3-3 0-3 3v2q0 4-4 4" fill="none" stroke="{B}" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>')
    # версии: дуга против часовой стрелки со стрелкой в конце и зелёная метка в центре
    icon("versions", f'<path class="ink" d="M14 32a18 18 0 1 0 5.3-12.7"/><path class="ink" d="M19.3 11.3v8h8"/><circle cx="32" cy="32" r="5" fill="{G}"/>')
    # приватность: щит с галочкой
    icon("privacy", f'<path class="ink" d="M32 8l20 7v15c0 13-9 22-20 26C21 52 12 43 12 30V15z"/><path d="M23 32l7 7 12-14" fill="none" stroke="{G}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>')
    # язык и тема: круг с закрашенной половиной, искра вынесена за его край
    icon("theme", f'<circle class="ink" cx="30" cy="34" r="19"/><path class="fill" d="M30 15a19 19 0 0 1 0 38z"/>{star(51, 13, 8, G)}')
    # отчёт: бумажный самолётик из двух граней
    icon("report", f'<path d="M56 8L8 27l18 8z" fill="{B}"/><path d="M56 8L26 35l8 19z" fill="#1d4ed8"/>')


def banner(lang: str) -> str:
    tagline = {"ru": "Оверлей для Windows, который убирает цветную каёмку текста на QD-OLED",
               "en": "A Windows overlay that removes colour fringing from text on QD-OLED"}[lang]
    chips = {"ru": ["Windows 10 2004+", "DXGI и Direct3D 11", "RU и EN"], "en": ["Windows 10 2004+", "DXGI and Direct3D 11", "RU and EN"]}[lang]
    chip_x, chip_svg = 330, []
    for text in chips:
        w = 14 + len(text) * 8.6
        chip_svg.append(f'<rect class="chip" x="{chip_x}" y="214" width="{w:.0f}" height="36" rx="18"/>'
                        f'<text class="t2" x="{chip_x + w / 2:.0f}" y="238" font-family="{FONT}" font-size="15" text-anchor="middle">{text}</text>')
        chip_x += w + 14
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1200 300" width="1200" height="300" role="img" aria-label="fringefix">
  {STYLE}
  <rect class="bg" width="1200" height="300" rx="22"/>
  <circle cx="108" cy="96" r="50" fill="{R}"/><circle cx="212" cy="96" r="50" fill="{B}"/><circle cx="160" cy="188" r="50" fill="{G}"/>
  <text class="t1" x="330" y="142" font-family="{FONT}" font-size="76" font-weight="700">fringefix</text>
  <text class="t2" x="332" y="190" font-family="{FONT}" font-size="22">{tagline}</text>
  {''.join(chip_svg)}
</svg>
"""


def pipeline(lang: str) -> str:
    t = {
        "ru": (["Рабочий стол", "Захват кадра", "Подпись кадра", "Шейдер", "Окно оверлея"],
               ["DWM собирает экран", "Desktop Duplication, DXGI", "пропуск эха своего окна", "сдвиг R, G, B и лапласиан", "поверх всего, мышь проходит, вне захвата"],
               ("Поток захвата и оверлея", "Главный поток: трей, окна настроек, обновления, отчёты", "Автопауза: игра на переднем плане освобождает устройство и окно")),
        "en": (["Desktop", "Frame capture", "Frame signature", "Shader", "Overlay window"],
               ["DWM composes the screen", "Desktop Duplication, DXGI", "skips its own echo", "R, G, B shift and laplacian", "topmost, click-through, outside capture"],
               ("Capture and overlay thread", "Main thread: tray, settings windows, updates, reports", "Auto-pause: a game in front releases the device and the window")),
    }[lang]
    names, notes, lanes = t
    boxes, arrows = [], []
    for i, (name, note) in enumerate(zip(names, notes)):
        x = 40 + i * 226
        color = [None, G, B, R, G][i]
        boxes.append(f'<rect class="box" x="{x}" y="60" width="190" height="104" rx="14"/>'
                     + (f'<rect x="{x}" y="60" width="8" height="104" rx="4" fill="{color}"/>' if color else "")
                     + f'<text class="t1" x="{x + 22}" y="98" font-family="{FONT}" font-size="19" font-weight="600">{name}</text>'
                     + "".join(f'<text class="t2" x="{x + 22}" y="{124 + n * 18}" font-family="{FONT}" font-size="13">{line}</text>'
                               for n, line in enumerate(wrap(note, 26))))
        if i < 4:
            arrows.append(f'<path class="arrow" d="M{x + 196} 112h24M{x + 212} 104l8 8-8 8"/>')
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1200 300" width="1200" height="300" role="img" aria-label="{' - '.join(names)}">
  {STYLE}
  <rect class="bg" width="1200" height="300" rx="18"/>
  {''.join(boxes)}{''.join(arrows)}
  <path class="arrow" d="M266 184v14h872v-14"/>
  <text class="t2" x="266" y="222" font-family="{FONT}" font-size="14">{lanes[0]}</text>
  <text class="t2" x="40" y="262" font-family="{FONT}" font-size="14">{lanes[1]}</text>
  <text class="t2" x="40" y="284" font-family="{FONT}" font-size="14">{lanes[2]}</text>
</svg>
"""


def wrap(text: str, width: int) -> list[str]:
    lines, line = [], ""
    for word in text.split(" "):
        if len(line) + len(word) + 1 > width and line:
            lines.append(line)
            line = word
        else:
            line = f"{line} {word}".strip()
    lines.append(line)
    return lines


def main() -> None:
    README.mkdir(parents=True, exist_ok=True)
    icons()
    for lang in ("ru", "en"):
        (README / f"banner-{lang}.svg").write_text(banner(lang), encoding="utf-8")
        (README / f"pipeline-{lang}.svg").write_text(pipeline(lang), encoding="utf-8")


if __name__ == "__main__":
    main()
