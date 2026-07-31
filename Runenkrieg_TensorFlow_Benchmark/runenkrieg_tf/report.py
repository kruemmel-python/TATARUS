from __future__ import annotations

import argparse
import csv
from pathlib import Path
from statistics import mean


def read_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(encoding="utf-8") as handle:
        return [row for row in csv.DictReader(handle) if row.get("game_win_rate")]


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge TensorFlow and TATARUS learning curves")
    parser.add_argument("--tensorflow", type=Path, default=Path("results/learning_curves.csv"))
    parser.add_argument("--tatarus", type=Path, default=Path("tatarus_measurements.csv"))
    parser.add_argument("--output", type=Path, default=Path("results/COMPARISON.md"))
    parser.add_argument("--target", type=float, default=0.60)
    args = parser.parse_args()
    rows = read_rows(args.tensorflow) + read_rows(args.tatarus)
    if not rows:
        raise SystemExit("Keine vollständigen Messzeilen gefunden.")
    rows.sort(key=lambda row: (row["agent"], int(row["environment_rounds"])))
    grouped: dict[tuple[str, int], list[dict[str, str]]] = {}
    for row in rows:
        grouped.setdefault(
            (row["agent"], int(row["environment_rounds"])),
            [],
        ).append(row)
    aggregate_rows: list[dict[str, str]] = []
    for (agent, environment_rounds), values in grouped.items():
        first = dict(values[0])
        first["agent"] = agent
        first["environment_rounds"] = str(environment_rounds)
        for field in (
            "game_win_rate",
            "decision_ms",
            "parameter_bytes",
            "mean_token_swing",
        ):
            first[field] = str(mean(float(row.get(field) or 0) for row in values))
        aggregate_rows.append(first)
    rows = sorted(
        aggregate_rows,
        key=lambda row: (row["agent"], int(row["environment_rounds"])),
    )
    first_target: dict[str, int | None] = {}
    for row in rows:
        agent = row["agent"]
        first_target.setdefault(agent, None)
        if first_target[agent] is None and float(row["game_win_rate"]) >= args.target:
            first_target[agent] = int(row["environment_rounds"])
    lines = [
        "# Runenkrieg-Architekturvergleich",
        "",
        f"Zielwert für Sample-Effizienz: Partie-Siegrate ≥ {args.target:.0%}.",
        "",
        "| Agent | Runden bis Ziel | letzter Holdout | Entscheidung ms | Parameterbytes |",
        "|---|---:|---:|---:|---:|",
    ]
    for agent in sorted(first_target):
        agent_rows = [row for row in rows if row["agent"] == agent]
        last = agent_rows[-1]
        reached = first_target[agent]
        lines.append(
            f"| {agent} | {reached if reached is not None else 'nicht erreicht'} "
            f"| {float(last['game_win_rate']):.1%} "
            f"| {float(last.get('decision_ms') or 0):.3f} "
            f"| {int(float(last.get('parameter_bytes') or 0))} |"
        )
    lines.extend(
        (
            "",
            "> Dieser Bericht ist deskriptiv. Eine Überlegenheitsbehauptung "
            "erfordert mehrere Trainingsseeds, Konfidenzintervalle und Replikation.",
        )
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()
