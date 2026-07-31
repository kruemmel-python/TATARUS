from __future__ import annotations

import json
from math import comb, sqrt
from pathlib import Path


ROOT = Path(__file__).resolve().parent
TATARUS = ROOT / "Runenkrieg_Tatarus_10k_Benchmark" / "exports" / "tatarus_frozen_winner.json"
CONVENTIONAL = ROOT / "Runenkrieg_TensorFlow_Benchmark" / "exports" / "runenkrieg_frozen_winner.json"


def wilson(successes: int, trials: int) -> tuple[float, float]:
    z = 1.959963984540054
    rate = successes / trials
    denominator = 1.0 + z * z / trials
    center = (rate + z * z / (2.0 * trials)) / denominator
    half = z * sqrt(
        rate * (1.0 - rate) / trials + z * z / (4.0 * trials * trials)
    ) / denominator
    return center - half, center + half


def fisher_two_sided(a: int, b: int, c: int, d: int) -> float:
    first_total = a + b
    second_total = c + d
    success_total = a + c
    total = first_total + second_total
    lower = max(0, success_total - second_total)
    upper = min(first_total, success_total)
    denominator = comb(total, first_total)
    probabilities = {
        successes: (
            comb(success_total, successes)
            * comb(total - success_total, first_total - successes)
            / denominator
        )
        for successes in range(lower, upper + 1)
    }
    observed = probabilities[a]
    return sum(value for value in probabilities.values() if value <= observed + 1e-15)


def main() -> None:
    tatarus = json.loads(TATARUS.read_text(encoding="utf-8"))["independent_replication"]
    conventional = json.loads(CONVENTIONAL.read_text(encoding="utf-8"))["independent_replication"]
    t_wins = int(tatarus["wins"])
    t_games = int(tatarus["games"])
    c_games = 50
    c_wins = round(float(conventional["game_win_rate"]) * c_games)
    t_interval = wilson(t_wins, t_games)
    c_interval = wilson(c_wins, c_games)
    difference = t_wins / t_games - c_wins / c_games
    difference_interval = (
        difference - sqrt(
            (t_wins / t_games - t_interval[0]) ** 2
            + (c_interval[1] - c_wins / c_games) ** 2
        ),
        difference + sqrt(
            (t_interval[1] - t_wins / t_games) ** 2
            + (c_wins / c_games - c_interval[0]) ** 2
        ),
    )
    result = {
        "tatarus": {"wins": t_wins, "games": t_games, "wilson95": t_interval},
        "conventional": {"wins": c_wins, "games": c_games, "wilson95": c_interval},
        "difference": difference,
        "newcombe95": difference_interval,
        "fisher_two_sided": fisher_two_sided(
            t_wins,
            t_games - t_wins,
            c_wins,
            c_games - c_wins,
        ),
        "strict_pairing": False,
        "reason": "Kotlin and Python episode streams are not bit-identical",
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
