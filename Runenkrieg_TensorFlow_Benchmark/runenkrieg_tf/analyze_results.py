from __future__ import annotations

import argparse
import csv
import itertools
import json
import math
from pathlib import Path
from statistics import mean

import numpy as np


METRICS = (
    "game_win_rate",
    "round_win_rate",
    "mean_token_swing",
    "mean_reward",
    "decision_ms",
    "training_cpu_seconds",
)


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def interval(values: list[float], seed: int) -> tuple[float, float]:
    data = np.asarray(values, dtype=np.float64)
    if len(data) == 1:
        return float(data[0]), float(data[0])
    rng = np.random.default_rng(seed)
    samples = rng.choice(data, size=(20_000, len(data)), replace=True).mean(axis=1)
    low, high = np.percentile(samples, [2.5, 97.5])
    return float(low), float(high)


def exact_paired_permutation(differences: list[float]) -> float:
    observed = abs(mean(differences))
    permutations = []
    for signs in itertools.product((-1.0, 1.0), repeat=len(differences)):
        permutations.append(abs(mean(value * sign for value, sign in zip(differences, signs))))
    return sum(value >= observed - 1e-12 for value in permutations) / len(permutations)


def holm_adjust(p_values: list[tuple[str, float]]) -> dict[str, float]:
    ordered = sorted(p_values, key=lambda item: item[1])
    adjusted: dict[str, float] = {}
    running = 0.0
    total = len(ordered)
    for index, (name, value) in enumerate(ordered):
        running = max(running, min(1.0, (total - index) * value))
        adjusted[name] = running
    return adjusted


def write_learning_curve_svg(
    aggregate: list[dict[str, str | int | float]],
    output: Path,
) -> None:
    width, height = 1200, 720
    left, right, top, bottom = 105, 35, 70, 95
    plot_width = width - left - right
    plot_height = height - top - bottom
    checkpoints = sorted({int(row["environment_rounds"]) for row in aggregate})
    agents = sorted({str(row["agent"]) for row in aggregate})
    colors = {
        "contextual_bandit": "#00a6a6",
        "dqn": "#f28e2b",
        "gru": "#4e79a7",
        "mlp": "#e15759",
        "ppo": "#9c6ade",
    }

    x_min, x_max = math.log10(min(checkpoints)), math.log10(max(checkpoints))
    y_min, y_max = 0.35, 0.85

    def x(value: int) -> float:
        return left + (math.log10(value) - x_min) / (x_max - x_min) * plot_width

    def y(value: float) -> float:
        clipped = max(y_min, min(y_max, value))
        return top + (y_max - clipped) / (y_max - y_min) * plot_height

    parts = [
        '<svg xmlns="http://www.w3.org/2000/svg" '
        f'width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        "<style>"
        "text{font-family:Arial,sans-serif;fill:#172033}"
        ".grid{stroke:#d8dee9;stroke-width:1}"
        ".axis{stroke:#596579;stroke-width:2}"
        ".tick{font-size:16px}"
        ".title{font-size:26px;font-weight:700}"
        ".subtitle{font-size:16px;fill:#596579}"
        ".legend{font-size:16px}"
        "</style>",
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        '<text class="title" x="105" y="34">'
        "Runenkrieg: vollständige Mehrseed-Lernkurve</text>",
        '<text class="subtitle" x="105" y="57">'
        "Partie-Siegrate, Mittelwert und 95-%-Bootstrapintervall über 5 Trainingsseeds"
        "</text>",
    ]

    for rate in (0.4, 0.5, 0.6, 0.7, 0.8):
        y_pos = y(rate)
        parts.extend(
            [
                f'<line class="grid" x1="{left}" y1="{y_pos:.1f}" '
                f'x2="{width - right}" y2="{y_pos:.1f}"/>',
                f'<text class="tick" x="{left - 14}" y="{y_pos + 5:.1f}" '
                f'text-anchor="end">{rate:.0%}</text>',
            ]
        )
    for checkpoint in checkpoints:
        x_pos = x(checkpoint)
        label = f"{checkpoint // 1000}k" if checkpoint >= 1000 else str(checkpoint)
        parts.extend(
            [
                f'<line class="grid" x1="{x_pos:.1f}" y1="{top}" '
                f'x2="{x_pos:.1f}" y2="{height - bottom}"/>',
                f'<text class="tick" x="{x_pos:.1f}" y="{height - bottom + 30}" '
                f'text-anchor="middle">{label}</text>',
            ]
        )
    parts.extend(
        [
            f'<line class="axis" x1="{left}" y1="{height - bottom}" '
            f'x2="{width - right}" y2="{height - bottom}"/>',
            f'<line class="axis" x1="{left}" y1="{top}" '
            f'x2="{left}" y2="{height - bottom}"/>',
            f'<text class="tick" x="{left + plot_width / 2:.1f}" '
            f'y="{height - 28}" text-anchor="middle">beobachtete Umweltrunden '
            "(logarithmische Achse)</text>",
            f'<text class="tick" transform="translate(28 {top + plot_height / 2:.1f}) '
            'rotate(-90)" text-anchor="middle">Holdout-Partie-Siegrate</text>',
        ]
    )

    for index, agent in enumerate(agents):
        rows = sorted(
            (row for row in aggregate if row["agent"] == agent),
            key=lambda row: int(row["environment_rounds"]),
        )
        color = colors.get(agent, "#333333")
        upper = [
            (x(int(row["environment_rounds"])), y(float(row["game_win_rate_ci95_high"])))
            for row in rows
        ]
        lower = [
            (x(int(row["environment_rounds"])), y(float(row["game_win_rate_ci95_low"])))
            for row in reversed(rows)
        ]
        band = " ".join(f"{px:.1f},{py:.1f}" for px, py in upper + lower)
        line = " ".join(
            f"{x(int(row['environment_rounds'])):.1f},"
            f"{y(float(row['game_win_rate_mean'])):.1f}"
            for row in rows
        )
        parts.append(
            f'<polygon points="{band}" fill="{color}" fill-opacity="0.13"/>'
        )
        parts.append(
            f'<polyline points="{line}" fill="none" stroke="{color}" '
            'stroke-width="4" stroke-linejoin="round"/>'
        )
        for row in rows:
            parts.append(
                f'<circle cx="{x(int(row["environment_rounds"])):.1f}" '
                f'cy="{y(float(row["game_win_rate_mean"])):.1f}" r="5" '
                f'fill="{color}"/>'
            )
        legend_x = 650 + (index % 3) * 175
        legend_y = 680 + (index // 3) * 24
        parts.extend(
            [
                f'<line x1="{legend_x}" y1="{legend_y - 5}" '
                f'x2="{legend_x + 28}" y2="{legend_y - 5}" '
                f'stroke="{color}" stroke-width="4"/>',
                f'<text class="legend" x="{legend_x + 36}" y="{legend_y}">'
                f"{agent}</text>",
            ]
        )
    parts.append("</svg>")
    output.write_text("\n".join(parts) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Aggregate the pre-registered learning curves")
    parser.add_argument("--results", type=Path, default=Path("results_full"))
    args = parser.parse_args()
    rows = read_rows(args.results / "learning_curves.csv")
    winner = json.loads((args.results / "winner.json").read_text(encoding="utf-8"))
    groups: dict[tuple[str, int], list[dict[str, str]]] = {}
    for row in rows:
        groups.setdefault(
            (row["agent"], int(row["environment_rounds"])),
            [],
        ).append(row)

    aggregate: list[dict[str, str | int | float]] = []
    for (agent, checkpoint), values in sorted(groups.items()):
        item: dict[str, str | int | float] = {
            "agent": agent,
            "environment_rounds": checkpoint,
            "training_seed_count": len(values),
        }
        for metric_index, metric in enumerate(METRICS):
            samples = [float(row[metric]) for row in values]
            low, high = interval(samples, 31_000 + checkpoint + metric_index)
            item[f"{metric}_mean"] = mean(samples)
            item[f"{metric}_ci95_low"] = low
            item[f"{metric}_ci95_high"] = high
        aggregate.append(item)

    aggregate_path = args.results / "aggregate_learning_curves.csv"
    with aggregate_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(aggregate[0]))
        writer.writeheader()
        writer.writerows(aggregate)
    write_learning_curve_svg(
        aggregate,
        args.results / "learning_curves_game_win_rate.svg",
    )

    final_rows = [row for row in rows if int(row["environment_rounds"]) == 10_000]
    by_agent_seed = {
        (row["agent"], int(row["seed"])): row
        for row in final_rows
    }
    winner_name = winner["winner_agent"]
    seeds = sorted(
        int(row["seed"]) for row in final_rows if row["agent"] == winner_name
    )
    tests: list[dict[str, float | str]] = []
    raw_p: list[tuple[str, float]] = []
    for competitor in sorted({row["agent"] for row in final_rows} - {winner_name}):
        differences = [
            float(by_agent_seed[(winner_name, seed)]["game_win_rate"])
            - float(by_agent_seed[(competitor, seed)]["game_win_rate"])
            for seed in seeds
        ]
        p_value = exact_paired_permutation(differences)
        raw_p.append((competitor, p_value))
        low, high = interval(differences, 73_000 + len(tests))
        tests.append(
            {
                "competitor": competitor,
                "mean_difference": mean(differences),
                "ci95_low": low,
                "ci95_high": high,
                "raw_p": p_value,
            }
        )
    adjusted = holm_adjust(raw_p)
    for test in tests:
        test["holm_p"] = adjusted[str(test["competitor"])]

    lines = [
        "# Statistischer Mehrseed-Bericht",
        "",
        "Protokoll: `RUNENKRIEG-TF-MULTISEED-1`",
        "",
        "![Fünfseed-Lernkurve](learning_curves_game_win_rate.svg)",
        "",
        "## Endcheckpoint bei 10.000 Umweltrunden",
        "",
        "| Agent | Partie-Siegrate, Mittel [95-%-KI] | Token-Differenz | Entscheidung ms |",
        "|---|---:|---:|---:|",
    ]
    for row in winner["ranking"]:
        aggregate_row = next(
            item
            for item in aggregate
            if item["agent"] == row["agent"]
            and item["environment_rounds"] == 10_000
        )
        lines.append(
            f"| {row['agent']} "
            f"| {aggregate_row['game_win_rate_mean']:.1%} "
            f"[{aggregate_row['game_win_rate_ci95_low']:.1%}; "
            f"{aggregate_row['game_win_rate_ci95_high']:.1%}] "
            f"| {aggregate_row['mean_token_swing_mean']:.2f} "
            f"| {aggregate_row['decision_ms_mean']:.3f} |"
        )
    lines.extend(
        [
            "",
            f"Ausgewählt wurde **{winner_name}**, Seed "
            f"**{winner['winner_training_seed']}**, strikt nach der "
            "vorregistrierten Gewinnerregel.",
            "",
            "## Gepaarte explorative Signifikanztests",
            "",
            "| Vergleich (Gewinner − Kontrolle) | Differenz | 95-%-KI | exaktes p | Holm-p |",
            "|---|---:|---:|---:|---:|",
        ]
    )
    for test in tests:
        lines.append(
            f"| {winner_name} − {test['competitor']} "
            f"| {test['mean_difference']:.3f} "
            f"| [{test['ci95_low']:.3f}; {test['ci95_high']:.3f}] "
            f"| {test['raw_p']:.4f} | {test['holm_p']:.4f} |"
        )
    replication = winner["independent_replication"]
    lines.extend(
        [
            "",
            "Bei nur fünf Trainingsseeds haben exakte Tests eine grobe "
            "Auflösung. Sie sind explorativ und begründen allein keine "
            "allgemeine Überlegenheitsbehauptung.",
            "",
            "## Unabhängige Replikation des eingefrorenen Gewinners",
            "",
            f"- Partie-Siegrate: {replication['game_win_rate']:.1%}",
            f"- Rundensiegrate: {replication['round_win_rate']:.1%}",
            f"- mittlere Token-Differenz: {replication['mean_token_swing']:.2f}",
            f"- mittlere Entscheidungszeit: {replication['decision_ms']:.3f} ms",
            "",
            "Die Replikationsseeds 60000–60049 wurden erst nach der Auswahl "
            "des Checkpoints ausgewertet.",
        ]
    )
    report = args.results / "STATISTICAL_REPORT.md"
    report.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(report, flush=True)


if __name__ == "__main__":
    main()
