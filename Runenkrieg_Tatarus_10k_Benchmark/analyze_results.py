from __future__ import annotations

import csv
import hashlib
import json
import math
import random
import shutil
from pathlib import Path
from statistics import mean


ROOT = Path(__file__).resolve().parent
RESULTS = ROOT / "results_full"
EXPORTS = ROOT / "exports"
REPLICATION = RESULTS / "independent_replication.json"
SEEDS = (20260730, 20260731, 20260732, 20260733, 20260734)
CHECKPOINTS = (250, 500, 1000, 2000, 5000, 10000)
METRICS = (
    "game_win_rate",
    "round_win_rate",
    "mean_token_swing",
    "decision_ms",
    "spikes_per_game",
    "transmissions_per_game",
    "energy_cost_per_game",
    "training_cpu_seconds",
    "training_wall_seconds",
)


def interval(values: list[float], seed: int) -> tuple[float, float]:
    if len(values) == 1:
        return values[0], values[0]
    rng = random.Random(seed)
    samples = sorted(
        mean(rng.choice(values) for _ in values)
        for _ in range(20_000)
    )
    return samples[499], samples[19_499]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_rows(require_complete: bool = True) -> list[dict]:
    rows: list[dict] = []
    for seed in SEEDS:
        curve = RESULTS / f"seed_{seed}" / "learning_curve.json"
        if not curve.exists():
            if require_complete:
                raise FileNotFoundError(curve)
            continue
        rows.extend(json.loads(curve.read_text(encoding="utf-8")))
    if require_complete:
        observed = {
            (int(row["seed"]), int(row["environment_rounds"]))
            for row in rows
        }
        expected = {(seed, checkpoint) for seed in SEEDS for checkpoint in CHECKPOINTS}
        missing = sorted(expected - observed)
        if missing:
            raise RuntimeError(f"Incomplete benchmark; missing {missing}")
    return rows


def write_csv(path: Path, rows: list[dict]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def aggregate_rows(rows: list[dict]) -> list[dict]:
    result: list[dict] = []
    checkpoints = sorted({int(row["environment_rounds"]) for row in rows})
    for checkpoint in checkpoints:
        group = [
            row for row in rows
            if int(row["environment_rounds"]) == checkpoint
        ]
        item: dict[str, int | float] = {
            "environment_rounds": checkpoint,
            "training_seed_count": len(group),
        }
        for metric_index, metric in enumerate(METRICS):
            values = [float(row[metric]) for row in group]
            low, high = interval(values, 81_000 + checkpoint + metric_index)
            item[f"{metric}_mean"] = mean(values)
            item[f"{metric}_ci95_low"] = low
            item[f"{metric}_ci95_high"] = high
        result.append(item)
    return result


def write_svg(aggregate: list[dict], output: Path) -> None:
    width, height = 1120, 680
    left, right, top, bottom = 100, 35, 75, 85
    plot_width = width - left - right
    plot_height = height - top - bottom
    x_min, x_max = math.log10(CHECKPOINTS[0]), math.log10(CHECKPOINTS[-1])
    y_min, y_max = 0.30, 0.90

    def x(value: int) -> float:
        return left + (math.log10(value) - x_min) / (x_max - x_min) * plot_width

    def y(value: float) -> float:
        return top + (y_max - value) / (y_max - y_min) * plot_height

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
        f'height="{height}" viewBox="0 0 {width} {height}">',
        "<style>text{font-family:Arial,sans-serif;fill:#172033}"
        ".grid{stroke:#d8dee9}.axis{stroke:#596579;stroke-width:2}"
        ".title{font-size:26px;font-weight:700}.sub{font-size:16px;fill:#596579}"
        ".tick{font-size:16px}</style>",
        '<rect width="100%" height="100%" fill="white"/>',
        '<text class="title" x="100" y="34">TATARUS LargeScale: '
        "10.000-Runden-Mehrseed-Lernkurve</text>",
        '<text class="sub" x="100" y="58">Partie-Siegrate; Mittelwert und '
        "95-%-Bootstrapintervall über fünf unabhängige Modelle</text>",
    ]
    for rate in (0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9):
        py = y(rate)
        parts.append(
            f'<line class="grid" x1="{left}" y1="{py:.1f}" '
            f'x2="{width-right}" y2="{py:.1f}"/>'
        )
        parts.append(
            f'<text class="tick" x="{left-12}" y="{py+5:.1f}" '
            f'text-anchor="end">{rate:.0%}</text>'
        )
    for checkpoint in CHECKPOINTS:
        px = x(checkpoint)
        label = f"{checkpoint // 1000}k" if checkpoint >= 1000 else str(checkpoint)
        parts.append(
            f'<line class="grid" x1="{px:.1f}" y1="{top}" '
            f'x2="{px:.1f}" y2="{height-bottom}"/>'
        )
        parts.append(
            f'<text class="tick" x="{px:.1f}" y="{height-bottom+29}" '
            f'text-anchor="middle">{label}</text>'
        )
    upper = [
        (x(int(row["environment_rounds"])), y(float(row["game_win_rate_ci95_high"])))
        for row in aggregate
    ]
    lower = [
        (x(int(row["environment_rounds"])), y(float(row["game_win_rate_ci95_low"])))
        for row in reversed(aggregate)
    ]
    band = " ".join(f"{px:.1f},{py:.1f}" for px, py in upper + lower)
    line = " ".join(
        f"{x(int(row['environment_rounds'])):.1f},"
        f"{y(float(row['game_win_rate_mean'])):.1f}"
        for row in aggregate
    )
    parts.extend(
        [
            f'<polygon points="{band}" fill="#00a6a6" fill-opacity="0.18"/>',
            f'<polyline points="{line}" fill="none" stroke="#008b8b" '
            'stroke-width="5" stroke-linejoin="round"/>',
        ]
    )
    for row in aggregate:
        parts.append(
            f'<circle cx="{x(int(row["environment_rounds"])):.1f}" '
            f'cy="{y(float(row["game_win_rate_mean"])):.1f}" r="6" '
            'fill="#008b8b"/>'
        )
    parts.extend(
        [
            f'<line class="axis" x1="{left}" y1="{height-bottom}" '
            f'x2="{width-right}" y2="{height-bottom}"/>',
            f'<line class="axis" x1="{left}" y1="{top}" '
            f'x2="{left}" y2="{height-bottom}"/>',
            f'<text class="tick" x="{left+plot_width/2:.1f}" y="{height-25}" '
            'text-anchor="middle">beobachtete Umweltrunden '
            "(logarithmische Achse)</text>",
            "</svg>",
        ]
    )
    output.write_text("\n".join(parts) + "\n", encoding="utf-8")


def main() -> None:
    rows = read_rows()
    write_csv(RESULTS / "learning_curves.csv", rows)
    aggregate = aggregate_rows(rows)
    write_csv(RESULTS / "aggregate_learning_curves.csv", aggregate)
    write_svg(aggregate, RESULTS / "learning_curves_game_win_rate.svg")

    final_rows = [
        row for row in rows
        if int(row["environment_rounds"]) == CHECKPOINTS[-1]
    ]
    ranking = sorted(
        final_rows,
        key=lambda row: (
            -float(row["game_win_rate"]),
            -float(row["mean_token_swing"]),
            float(row["decision_ms"]),
            int(row["seed"]),
        ),
    )
    winner = ranking[0]
    winner_seed = int(winner["seed"])
    source = (
        RESULTS / f"seed_{winner_seed}" / "round_10000"
        / f"tatarus_seed_{winner_seed}_round_10000.json.gz"
    )
    EXPORTS.mkdir(exist_ok=True)
    target = EXPORTS / "tatarus_frozen_winner.json.gz"
    shutil.copyfile(source, target)
    replication = (
        json.loads(REPLICATION.read_text(encoding="utf-8"))
        if REPLICATION.exists()
        else None
    )
    if replication is not None:
        if int(replication["training_seed"]) != winner_seed:
            raise RuntimeError("Replication seed does not match selected winner")
        if replication["snapshot_sha256"] != sha256(target):
            raise RuntimeError("Replication snapshot hash does not match winner")
        if not replication["learning_disabled"]:
            raise RuntimeError("Replication was not learning-free")
        if not replication["state_unchanged_after_evaluation"]:
            raise RuntimeError("Winner state changed during replication")

    metadata = {
        "protocol": "RUNENKRIEG-TATARUS-MULTISEED-1",
        "agent": "tatarus_large_scale",
        "training_seed": winner_seed,
        "selection_checkpoint": 10000,
        "selection_rule": [
            "highest game_win_rate",
            "highest mean_token_swing",
            "lowest decision_ms",
            "lowest seed",
        ],
        "holdout": winner,
        "sha256": sha256(target),
        "bytes": target.stat().st_size,
        "learning_on_android": False,
        "independent_replication": replication,
    }
    (EXPORTS / "tatarus_frozen_winner.json").write_text(
        json.dumps(metadata, indent=2),
        encoding="utf-8",
    )

    lines = [
        "# TATARUS-Mehrseed-Bericht",
        "",
        "Protokoll: `RUNENKRIEG-TATARUS-MULTISEED-1`",
        "",
        "![Lernkurve](learning_curves_game_win_rate.svg)",
        "",
        "| Runden | Partiensiegrate [95-%-KI] | Rundensiegrate "
        "| Token-Differenz | Entscheidung ms |",
        "|---:|---:|---:|---:|---:|",
    ]
    for row in aggregate:
        lines.append(
            f"| {row['environment_rounds']} "
            f"| {row['game_win_rate_mean']:.1%} "
            f"[{row['game_win_rate_ci95_low']:.1%}; "
            f"{row['game_win_rate_ci95_high']:.1%}] "
            f"| {row['round_win_rate_mean']:.1%} "
            f"| {row['mean_token_swing_mean']:.2f} "
            f"| {row['decision_ms_mean']:.3f} |"
        )
    lines.extend(
        [
            "",
            f"Vorregistriert ausgewählt: Seed **{winner_seed}** am "
            "10.000er-Checkpoint.",
            "",
            (
                "Unabhängige Replikation auf Seeds 60000–60049: "
                f"**{replication['game_win_rate']:.1%}** Partiensiegrate, "
                f"Token-Differenz **{replication['mean_token_swing']:.2f}**, "
                "Modellzustand unverändert."
                if replication is not None
                else "Die unabhängige Replikation auf Seeds 60000–60049 "
                "wird erst nach dieser Auswahl eingetragen."
            ),
        ]
    )
    (RESULTS / "STATISTICAL_REPORT.md").write_text(
        "\n".join(lines) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
