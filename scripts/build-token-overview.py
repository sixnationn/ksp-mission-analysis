"""Build the current token overview from recorded session counters."""

from __future__ import annotations

import html
import json
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MILESTONES = ROOT / "benchmarks" / "milestones"
REPORT = ROOT / "reports" / "token-overview"
FIELDS = ("input_tokens", "cached_input_tokens", "output_tokens", "reasoning_output_tokens")


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def totals(rows: list[dict]) -> dict:
    value = {field: sum(row.get(field, 0) for row in rows) for field in FIELDS}
    value["fresh_input_tokens"] = value["input_tokens"] - value["cached_input_tokens"]
    if value["fresh_input_tokens"] < 0 or value["reasoning_output_tokens"] > value["output_tokens"]:
        raise ValueError("Token subsets do not reconcile")
    return value


def timestamp(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def short(value: int) -> str:
    if value >= 1_000_000:
        return f"{value / 1_000_000:.2f}M"
    if value >= 1_000:
        return f"{value / 1_000:.1f}k"
    return str(value)


def label(x: float, y: float, value: str, size: int = 12, color: str = "#aaa",
          anchor: str = "start", weight: int = 400) -> str:
    return (f'<text x="{x:.2f}" y="{y:.2f}" fill="{color}" font-size="{size}" '
            f'font-weight="{weight}" text-anchor="{anchor}" '
            f'font-family="Arial, Helvetica, sans-serif">{html.escape(str(value))}</text>')


def line(x1: float, y1: float, x2: float, y2: float, color: str,
         width: float = 1, opacity: float = 1) -> str:
    return (f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
            f'stroke="{color}" stroke-width="{width}" stroke-opacity="{opacity}"/>')


def input_chart(models: list[dict]) -> str:
    left, right = 162, 664
    maximum = max(50_000_000, ((models[0]["input_tokens"] + 49_999_999)
                               // 50_000_000) * 50_000_000)
    width = right - left
    grid = []
    for index in range(5):
        value = round(maximum * index / 4)
        x = left + width * index / 4
        grid.extend((line(x, 27, x, 299, "#606060", opacity=.7 if index == 0 else .24),
                     label(x, 319, f"{value / 1_000_000:.0f}M",
                           color="#a7a7a7", anchor="middle")))
    bars = []
    for index, row in enumerate(models):
        y = 64 + index * 55
        cached_end = left + width * row["cached_input_tokens"] / maximum
        total_end = left + width * row["input_tokens"] / maximum
        bars.extend((
            label(0, y + 5, f'{row["model"].replace("gpt-6-", "")} · {row["effort"]}',
                  15, "#eee", weight=600),
            line(left, y, cached_end, y, "#dedede", 4),
            line(cached_end, y, total_end, y, "#f2c679", 5),
            f'<circle cx="{total_end:.2f}" cy="{y}" r="4" fill="#f2c679"/>',
            label(744, y + 5, short(row["input_tokens"]), 14, "#f1f1f1", "end", 600)
        ))
    return "\n".join((
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 346" role="img" '
        'aria-labelledby="input-title input-desc">',
        '<title id="input-title">Recorded input by model and reasoning effort</title>',
        '<desc id="input-desc">Five model and effort groups. Gray denotes cached input, amber denotes fresh input. Sol xhigh has the largest total.</desc>',
        '<rect width="760" height="346" fill="#050505"/>',
        *grid, *bars,
        line(left, 299, right, 299, "#999", opacity=.55),
        label((left + right) / 2, 341, "Total recorded input · millions of tokens",
              12, "#b8b8b8", "middle"),
        "</svg>"
    ))


def growth_chart(timeline: list[dict]) -> str:
    left, right, top, bottom = 62, 688, 31, 222
    first = timestamp(timeline[0]["captured_at_utc"])
    last = timestamp(timeline[-1]["captured_at_utc"])
    duration = (last - first).total_seconds()
    maximum = max(1_000_000, ((timeline[-1]["fresh_input_tokens"] + 999_999)
                              // 1_000_000) * 1_000_000)

    def x(item: dict) -> float:
        elapsed = (timestamp(item["captured_at_utc"]) - first).total_seconds()
        return left + elapsed / duration * (right - left)

    def y(value: int) -> float:
        return bottom - value / maximum * (bottom - top)

    grid = []
    for part in (0, .25, .5, .75, 1):
        yy = y(round(maximum * part))
        grid.extend((line(left, yy, right, yy, "#666", opacity=.55 if part == 0 else .2),
                     label(left - 10, yy + 4, short(round(maximum * part)),
                           11, "#aaa", "end")))
    ticks = []
    for part in (0, 1 / 3, 2 / 3, 1):
        xx = left + (right - left) * part
        moment = first + (last - first) * part
        ticks.extend((line(xx, bottom, xx, bottom + 5, "#888"),
                      label(xx, 245, moment.strftime("%d %b %H:%M"),
                            11, "#aaa", "middle")))

    def points(field: str) -> str:
        return " ".join(f'{x(item):.2f},{y(item[field]):.2f}' for item in timeline)

    fresh = timeline[-1]["fresh_input_tokens"]
    output = timeline[-1]["output_tokens"]
    return "\n".join((
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 285" role="img" '
        'aria-labelledby="growth-title growth-desc">',
        '<title id="growth-title">Cumulative fresh input and output after the first benchmark baseline</title>',
        f'<desc id="growth-desc">The checkpoint record grows to {short(fresh)} fresh input '
        f'and {short(output)} output tokens after the first baseline. Planning tokens '
        'before that baseline are included in the model totals above.</desc>',
        '<rect width="760" height="285" fill="#050505"/>',
        *grid, *ticks,
        f'<polyline points="{points("fresh_input_tokens")}" fill="none" stroke="#f2c679" '
        'stroke-width="2.5" stroke-linejoin="round"/>',
        f'<polyline points="{points("output_tokens")}" fill="none" stroke="#b7b5de" '
        'stroke-width="2.5" stroke-linejoin="round"/>',
        f'<circle cx="{x(timeline[-1]):.2f}" cy="{y(fresh):.2f}" r="4" fill="#f2c679"/>',
        f'<circle cx="{x(timeline[-1]):.2f}" cy="{y(output):.2f}" r="4" fill="#b7b5de"/>',
        label(700, y(fresh) + 4, short(fresh), 12, "#f2c679", weight=600),
        label(700, y(output) + 4, short(output), 12, "#b7b5de", weight=600),
        label((left + right) / 2, 277, "Checkpoint time · UTC", 12, "#b8b8b8", "middle"),
        "</svg>"
    ))


def main() -> None:
    ledger = read_json(ROOT / "benchmarks" / "token-usage.json")
    usage = read_json(REPORT / "account-usage.json")
    if usage["used_percent"] + usage["remaining_percent"] != 100:
        raise ValueError("Account usage percentages do not reconcile")
    models = []
    for row in ledger["models"]:
        fresh = row["input_tokens"] - row["cached_input_tokens"]
        if fresh != row["uncached_input_tokens"] or row["reasoning_output_tokens"] > row["output_tokens"]:
            raise ValueError("Model accounting does not reconcile")
        models.append({
            "model": row["model"], "effort": row["reasoning_effort"],
            "input_tokens": row["input_tokens"],
            "cached_input_tokens": row["cached_input_tokens"],
            "fresh_input_tokens": fresh, "output_tokens": row["output_tokens"],
            "reasoning_output_tokens": row["reasoning_output_tokens"]
        })
    models.sort(key=lambda row: row["input_tokens"], reverse=True)
    overall = totals(ledger["models"])
    start = read_json(MILESTONES / "start.json")
    baseline = totals(start["sessions"])
    checkpoints = [
        (item.stem, read_json(item)) for item in MILESTONES.glob("*.json")
        if item.name != "start.json" and not item.name.endswith("-start.json")
    ]
    checkpoints.sort(key=lambda item: timestamp(item[1]["generated_at_utc"]))
    snapshots = [("First baseline", start), *checkpoints, ("Current", ledger)]
    timeline = []
    for name, snapshot in snapshots:
        value = totals(snapshot.get("models", snapshot.get("sessions", [])))
        timeline.append({
            "label": name, "captured_at_utc": snapshot["generated_at_utc"],
            "fresh_input_tokens": value["fresh_input_tokens"] - baseline["fresh_input_tokens"],
            "output_tokens": value["output_tokens"] - baseline["output_tokens"]
        })
    for before, after in zip(timeline, timeline[1:]):
        if (after["fresh_input_tokens"] < before["fresh_input_tokens"] or
                after["output_tokens"] < before["output_tokens"]):
            raise ValueError("Cumulative checkpoint counters moved backward")
    if timeline[-1]["fresh_input_tokens"] != overall["fresh_input_tokens"] - baseline["fresh_input_tokens"]:
        raise ValueError("Timeline endpoint does not reconcile")

    input_svg = input_chart(models)
    growth_svg = growth_chart(timeline)
    rows = "\n".join(
        f'<tr><th scope="row">{html.escape(row["model"])} · {html.escape(row["effort"])}</th>'
        f'<td>{row["input_tokens"]:,}</td><td>{row["cached_input_tokens"]:,}</td>'
        f'<td>{row["fresh_input_tokens"]:,}</td><td>{row["output_tokens"]:,}</td>'
        f'<td>{row["reasoning_output_tokens"]:,}</td></tr>' for row in models
    )
    pretty = lambda iso: timestamp(iso).strftime("%d %b %Y %H:%M")
    values = {
        "CAPTURED": pretty(ledger["generated_at_utc"]),
        "USAGE_CAPTURED": pretty(usage["captured_at_utc"]),
        "USAGE_RESET": pretty(usage["resets_at_utc"]),
        "USAGE_USED": str(usage["used_percent"]),
        "USAGE_REMAINING": str(usage["remaining_percent"]),
        "INPUT_TOTAL": f'{overall["input_tokens"]:,}',
        "CACHED_TOTAL": f'{overall["cached_input_tokens"]:,}',
        "FRESH_TOTAL": f'{overall["fresh_input_tokens"]:,}',
        "OUTPUT_TOTAL": f'{overall["output_tokens"]:,}',
        "REASONING_TOTAL": f'{overall["reasoning_output_tokens"]:,}',
        "CACHE_SHARE": f'{overall["cached_input_tokens"] / overall["input_tokens"] * 100:.1f}%',
        "INPUT_SVG": input_svg, "GROWTH_SVG": growth_svg, "TABLE_ROWS": rows
    }
    page = (REPORT / "template.html").read_text(encoding="utf-8")
    for key, value in values.items():
        page = page.replace("{{" + key + "}}", value)
    if "{{" in page:
        raise ValueError("Unfilled page placeholder")
    (REPORT / "index.html").write_text(page, encoding="utf-8")
    (REPORT / "recorded-input.svg").write_text(input_svg, encoding="utf-8")
    (REPORT / "checkpoint-growth.svg").write_text(growth_svg, encoding="utf-8")
    (REPORT / "data.json").write_text(json.dumps({
        "source": "benchmarks/token-usage.json",
        "captured_at_utc": ledger["generated_at_utc"],
        "model_totals": models, "overall": overall,
        "first_baseline_at_utc": start["generated_at_utc"],
        "timeline": timeline, "account_usage_snapshot": usage
    }, indent=2) + "\n", encoding="utf-8")
    print(f'Built {REPORT}')
    print(f'Input {overall["input_tokens"]:,}; cached {overall["cached_input_tokens"]:,}; '
          f'fresh {overall["fresh_input_tokens"]:,}; output {overall["output_tokens"]:,}; '
          f'reasoning {overall["reasoning_output_tokens"]:,}')


if __name__ == "__main__":
    main()
