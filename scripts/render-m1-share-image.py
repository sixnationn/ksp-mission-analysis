"""Render the M1 editorial share image from the frozen report data.

Uses the Pillow library already bundled with this Codex workspace. This is a
static companion to template.html, not a browser screenshot of that file.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent.parent
REPORT = ROOT / "reports" / "m1-showcase"
DATA = json.loads((REPORT / "data.json").read_text(encoding="utf-8"))
USAGE = json.loads((REPORT / "account-usage.json").read_text(encoding="utf-8"))
SCALE = 2
WIDTH, HEIGHT = 1600, 900

BACKGROUND = "#000000"
WHITE = "#f4f4f4"
SECONDARY = "#c7c7c7"
QUIET = "#929292"
RULE = "#363636"
FRESH = "#f2c679"
OUTPUT = "#b7b5de"


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "arialbd.ttf" if bold else "arial.ttf"
    return ImageFont.truetype(str(Path("C:/Windows/Fonts") / name), size * SCALE)


def at(value: float) -> int:
    return round(value * SCALE)


image = Image.new("RGB", (WIDTH * SCALE, HEIGHT * SCALE), BACKGROUND)
draw = ImageDraw.Draw(image)


def text(x: float, y: float, value: str, size: int = 14, color: str = WHITE, bold: bool = False):
    draw.text((at(x), at(y)), value, font=font(size, bold), fill=color, anchor="lt")


def right(x: float, y: float, value: str, size: int = 14, color: str = WHITE, bold: bool = False):
    draw.text((at(x), at(y)), value, font=font(size, bold), fill=color, anchor="rt")


def rule(x1: float, y1: float, x2: float, y2: float, color: str = RULE, width: float = 1):
    draw.line((at(x1), at(y1), at(x2), at(y2)), fill=color, width=max(1, at(width)))


def circle(x: float, y: float, radius: float, color: str):
    draw.ellipse((at(x - radius), at(y - radius), at(x + radius), at(y + radius)), fill=color)


def short(value: int) -> str:
    return f"{value / 1_000_000:.2f}M" if value >= 1_000_000 else f"{value / 1_000:.1f}k"


def pretty_utc(iso: str) -> str:
    return datetime.fromisoformat(iso.replace("Z", "+00:00")).astimezone(timezone.utc).strftime("%d %b %H:%M")


# Restrained article header and reading column.
text(34, 22, "KSP mission analysis", 13, WHITE, True)
right(1565, 23, "Engineering note  ·  22 September 2026", 12, SECONDARY)
rule(0, 58, 1600, 58, "#242424")

MAIN_X = 405
text(MAIN_X, 99, "M1 / SYSTEM IMPORT", 12, QUIET)
text(MAIN_X, 129, "First system import", 54, WHITE)
text(MAIN_X, 204, "JNSQ Reborn at Real scale now produces a provisional 32-body catalog", 18, SECONDARY)
text(MAIN_X, 231, "for either Principia branch. The importer passed 24 of 24 focused checks.", 18, SECONDARY)

# The side rail echoes the source article's low-emphasis section navigation.
RAIL_X = 116
text(RAIL_X, 282, "In this report", 12, WHITE, True)
for index, label in enumerate(("Overview", "Token record", "M1 result", "Checkpoint intervals")):
    text(RAIL_X, 313 + index * 28, label, 12, QUIET)
rule(RAIL_X, 442, RAIL_X + 204, 442)
text(RAIL_X, 462, "Codex account usage", 12, WHITE, True)
text(RAIL_X, 489, f"{USAGE['used_percent']}% used", 24, WHITE)
rule(RAIL_X, 530, RAIL_X + 204, 530, RULE, 3)
rule(RAIL_X, 530, RAIL_X + 204 * USAGE["used_percent"] / 100, 530, FRESH, 3)
text(RAIL_X, 546, f"{USAGE['remaining_percent']}% remaining · seven-day window", 11, QUIET)
text(RAIL_X, 564, f"Resets {pretty_utc(USAGE['resets_at_utc'])} UTC", 11, QUIET)
text(RAIL_X, 582, f"Captured {pretty_utc(USAGE['captured_at_utc'])} UTC", 11, QUIET)

# Main chart frame, axes, direct labels, and source-like thin series lines.
CARD_X, CARD_Y, CARD_W, CARD_H = MAIN_X, 276, 790, 368
draw.rounded_rectangle((at(CARD_X), at(CARD_Y), at(CARD_X + CARD_W), at(CARD_Y + CARD_H)),
                       radius=at(13), fill="#050505", outline="#3c3c3c", width=at(1))
text(CARD_X + 21, CARD_Y + 20, "Recorded input by assignment", 18, WHITE, True)
circle(958, 307, 4, "#dedede")
text(969, 298, "Cached input", 12, SECONDARY)
circle(1084, 307, 4, FRESH)
text(1095, 298, "Fresh input", 12, SECONDARY)

PLOT_LEFT, PLOT_RIGHT = 590, 1100
PLOT_TOP, PLOT_BOTTOM = 340, 576
for tick in (0, 5, 10, 15, 20):
    x = PLOT_LEFT + (PLOT_RIGHT - PLOT_LEFT) * tick / 20
    rule(x, PLOT_TOP, x, PLOT_BOTTOM, "#262626" if tick else "#666666")
    text(x - 9 if tick else x - 5, 586, f"{tick}M", 12, QUIET)
rule(PLOT_LEFT, PLOT_BOTTOM, PLOT_RIGHT, PLOT_BOTTOM, "#707070")

for index, role in enumerate(DATA["roles"]):
    y = 373 + index * 74
    text(CARD_X + 22, y - 15, f"Sol · {role['effort']}", 16, WHITE)
    text(CARD_X + 22, y + 8, role["label"].lower(), 12, QUIET)
    cached_end = PLOT_LEFT + (PLOT_RIGHT - PLOT_LEFT) * role["cached_input_tokens"] / 20_000_000
    total_end = PLOT_LEFT + (PLOT_RIGHT - PLOT_LEFT) * role["input_tokens"] / 20_000_000
    rule(PLOT_LEFT, y, cached_end, y, "#dedede", 4)
    rule(cached_end, y, total_end, y, FRESH, 5)
    circle(total_end, y, 4, FRESH)
    right(CARD_X + CARD_W - 24, y - 13, short(role["input_tokens"]), 15, WHITE, True)

text(736, 611, "Total recorded input · millions of tokens", 12, SECONDARY)
rule(CARD_X + 21, 621, CARD_X + CARD_W - 21, 621, "#292929")
text(CARD_X + 21, 629, "Sol assignments through the pinned M1 checkpoint; amber marks fresh input.", 12, SECONDARY)

# Compact evidence table rather than headline metric cards.
text(MAIN_X, 668, "At the M1 checkpoint", 21, WHITE)
overall = DATA["overall"]
cache_share = DATA["cache_share_percent"]
text(MAIN_X, 699, f"{overall['input_tokens']:,} input tokens; {cache_share:.1f}% cached. Output: {overall['output_tokens']:,} tokens.", 14, SECONDARY)

TABLE_TOP = 737
rule(MAIN_X, TABLE_TOP, MAIN_X + 790, TABLE_TOP, "#505050")
text(MAIN_X, TABLE_TOP + 9, "Assignment", 13, SECONDARY, True)
right(779, TABLE_TOP + 9, "Cached", 13, SECONDARY, True)
right(900, TABLE_TOP + 9, "Fresh", 13, SECONDARY, True)
right(1026, TABLE_TOP + 9, "Output", 13, SECONDARY, True)
right(1195, TABLE_TOP + 9, "Reasoning*", 13, SECONDARY, True)
rule(MAIN_X, TABLE_TOP + 35, MAIN_X + 790, TABLE_TOP + 35)
for index, role in enumerate(DATA["roles"]):
    y = TABLE_TOP + 44 + index * 31
    text(MAIN_X, y, f"Sol · {role['effort']}", 13, WHITE)
    right(779, y, short(role["cached_input_tokens"]), 13, WHITE)
    right(900, y, short(role["fresh"]), 13, WHITE)
    right(1026, y, short(role["output_tokens"]), 13, WHITE)
    right(1195, y, short(role["reasoning_output_tokens"]), 13, WHITE)
    rule(MAIN_X, y + 25, MAIN_X + 790, y + 25)
text(MAIN_X, 880, "*Reasoning is part of output. Account usage is separate from project token counts.", 11, QUIET)

image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS).save(REPORT / "screenshot.png", optimize=True)
print(f"Rendered {REPORT / 'screenshot.png'}")
