from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import numpy as np

from .agents import create_agent
from .experiment import evaluate


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def bootstrap_interval(values: list[float], seed: int = 23001) -> tuple[float, float]:
    data = np.asarray(values, dtype=np.float64)
    if len(data) == 1:
        return float(data[0]), float(data[0])
    rng = np.random.default_rng(seed)
    samples = rng.choice(data, size=(20_000, len(data)), replace=True).mean(axis=1)
    low, high = np.percentile(samples, [2.5, 97.5])
    return float(low), float(high)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Select, replicate and freeze the pre-registered winner"
    )
    parser.add_argument("--results", type=Path, default=Path("results_full"))
    parser.add_argument("--checkpoint", type=int, default=10_000)
    parser.add_argument("--replication-games", type=int, default=50)
    args = parser.parse_args()

    rows = [
        row
        for row in read_rows(args.results / "learning_curves.csv")
        if int(row["environment_rounds"]) == args.checkpoint
    ]
    if not rows:
        raise SystemExit("Keine vollständigen Endcheckpoint-Messungen gefunden.")
    grouped: dict[str, list[dict[str, str]]] = {}
    for row in rows:
        grouped.setdefault(row["agent"], []).append(row)

    ranking: list[dict] = []
    for agent_name, agent_rows in grouped.items():
        wins = [float(row["game_win_rate"]) for row in agent_rows]
        swings = [float(row["mean_token_swing"]) for row in agent_rows]
        latency = [float(row["decision_ms"]) for row in agent_rows]
        ci_low, ci_high = bootstrap_interval(wins)
        ranking.append(
            {
                "agent": agent_name,
                "training_seed_count": len(agent_rows),
                "mean_game_win_rate": float(np.mean(wins)),
                "game_win_rate_ci95_low": ci_low,
                "game_win_rate_ci95_high": ci_high,
                "mean_token_swing": float(np.mean(swings)),
                "mean_decision_ms": float(np.mean(latency)),
            }
        )
    ranking.sort(
        key=lambda row: (
            -row["mean_game_win_rate"],
            -row["mean_token_swing"],
            row["mean_decision_ms"],
            row["agent"],
        )
    )
    winner_name = ranking[0]["agent"]
    winner_candidates = grouped[winner_name]
    winner_candidates.sort(
        key=lambda row: (
            -float(row["game_win_rate"]),
            -float(row["mean_token_swing"]),
            float(row["decision_ms"]),
            int(row["seed"]),
        )
    )
    winner_seed = int(winner_candidates[0]["seed"])
    checkpoint_directory = (
        args.results
        / "models"
        / f"seed_{winner_seed}"
        / winner_name
        / f"round_{args.checkpoint}"
    )
    agent = create_agent(winner_name, winner_seed)
    agent.load(checkpoint_directory)
    replication = evaluate(
        agent,
        range(60_000, 60_000 + args.replication_games),
    )
    winner = {
        "protocol": "RUNENKRIEG-TF-MULTISEED-1",
        "selection_checkpoint": args.checkpoint,
        "selection_rule": [
            "highest mean game_win_rate",
            "highest mean_token_swing",
            "lowest mean decision_ms",
            "lexicographic agent name",
        ],
        "ranking": ranking,
        "winner_agent": winner_name,
        "winner_training_seed": winner_seed,
        "checkpoint_directory": checkpoint_directory.as_posix(),
        "independent_replication_seeds": [
            60_000,
            60_000 + args.replication_games - 1,
        ],
        "independent_replication": replication,
    }
    output = args.results / "winner.json"
    output.write_text(json.dumps(winner, indent=2), encoding="utf-8")
    print(json.dumps(winner, indent=2), flush=True)


if __name__ == "__main__":
    main()
