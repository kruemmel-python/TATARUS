from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
import platform
import random
import time

import numpy as np

from .agents import Agent, create_agent
from .encoding import encode
from .environment import RunenkriegEnv, Winner


CHECKPOINTS = (250, 500, 1_000, 2_000, 5_000, 10_000)
AGENTS = ("mlp", "gru", "dqn", "ppo", "contextual_bandit")
TRAINING_SEEDS = (20260730, 20260731, 20260732, 20260733, 20260734)


def run_round(
    agent: Agent,
    env: RunenkriegEnv,
    training: bool,
    rng: random.Random,
) -> tuple[float, Winner, float, bool]:
    player, weather, actions = env.round_context(
        "mixed" if training else "random"
    )
    features = np.stack([encode(env.state, player.card, action, weather) for action in actions])
    started = time.perf_counter_ns()
    choice = agent.select(features, training, rng)
    decision_ms = (time.perf_counter_ns() - started) / 1_000_000
    reward, winner = env.step(player, actions[choice.index], weather)
    game_done = env.state.done
    if game_done or not training:
        next_features = np.empty((0, 128), np.float32)
    else:
        random_state = env.rng.getstate()
        next_player, next_weather, next_actions = env.round_context("mixed")
        next_features = np.stack(
            [encode(env.state, next_player.card, action, next_weather) for action in next_actions]
        )
        env.rng.setstate(random_state)
    if training:
        agent.learn(choice, reward, next_features, game_done)
    if game_done and training:
        env.reset()
        agent.reset_episode()
    return reward, winner, decision_ms, game_done


def evaluate(agent: Agent, seeds: range, rule_sign: float = 1.0) -> dict[str, float]:
    rewards: list[float] = []
    decisions: list[float] = []
    wins = draws = losses = 0
    games_won = games_drawn = 0
    token_swings: list[int] = []
    for seed in seeds:
        env = RunenkriegEnv(seed, rule_sign=rule_sign)
        rng = random.Random(seed ^ 0x5446)
        agent.reset_episode()
        while True:
            reward, winner, decision_ms, game_done = run_round(
                agent, env, False, rng
            )
            rewards.append(reward)
            decisions.append(decision_ms)
            wins += winner == Winner.AI
            draws += winner == Winner.DRAW
            losses += winner == Winner.PLAYER
            if game_done:
                break
        token_swings.append(env.state.ai_tokens - env.state.player_tokens)
        games_won += env.state.ai_tokens > env.state.player_tokens
        games_drawn += env.state.ai_tokens == env.state.player_tokens
    rounds = max(1, wins + draws + losses)
    return {
        "round_win_rate": wins / rounds,
        "round_draw_rate": draws / rounds,
        "game_win_rate": games_won / max(1, len(seeds)),
        "game_draw_rate": games_drawn / max(1, len(seeds)),
        "mean_reward": float(np.mean(rewards)),
        "mean_token_swing": float(np.mean(token_swings)),
        "decision_ms": float(np.mean(decisions)),
        "decision_p95_ms": float(np.percentile(decisions, 95)),
    }


def train_agent(
    name: str,
    seed: int,
    output: Path,
    checkpoints: tuple[int, ...],
    evaluation_games: int,
) -> list[dict]:
    agent = create_agent(name, seed)
    env = RunenkriegEnv(seed)
    rng = random.Random(seed ^ 0xA11CE)
    rows: list[dict] = []
    trained = 0
    training_cpu = 0.0
    for checkpoint in checkpoints:
        started = time.process_time()
        while trained < checkpoint:
            run_round(agent, env, True, rng)
            trained += 1
        training_cpu += time.process_time() - started
        metrics = evaluate(agent, range(30_000, 30_000 + evaluation_games))
        switched = evaluate(
            agent,
            range(40_000, 40_000 + evaluation_games),
            rule_sign=-1.0,
        )
        probe_env = RunenkriegEnv(51_000)
        probe_player, probe_weather, probe_actions = probe_env.round_context("mixed")
        probe = np.stack(
            [
                encode(probe_env.state, probe_player.card, action, probe_weather)
                for action in probe_actions
            ]
        )
        row = {
            "agent": name,
            "seed": seed,
            "environment_rounds": checkpoint,
            "training_cpu_seconds": training_cpu,
            "parameter_bytes": agent.parameter_bytes(),
            "history_sensitivity": agent.history_sensitivity(probe),
            **metrics,
            "rule_switch_win_rate_before_adaptation": switched["round_win_rate"],
        }
        rows.append(row)
        checkpoint_dir = output / "models" / f"seed_{seed}" / name / f"round_{checkpoint}"
        agent.save(checkpoint_dir)
        reloaded = create_agent(name, seed)
        reloaded.load(checkpoint_dir)
        before_pause = agent.select(probe, False, random.Random(1)).index
        after_pause = reloaded.select(probe, False, random.Random(1)).index
        row["retention_action_equal"] = int(before_pause == after_pause)
        (checkpoint_dir / "metrics.json").write_text(
            json.dumps(row, indent=2), encoding="utf-8"
        )
        print(json.dumps(row, ensure_ascii=False), flush=True)
        env.reset()
        agent.reset_episode()
    return rows


def rule_change_adaptation(
    agent_name: str,
    seed: int,
    model_directory: Path,
    output: Path,
    evaluation_games: int,
    checkpoints: tuple[int, ...] = (0, 250, 500, 1_000),
) -> list[dict]:
    agent = create_agent(agent_name, seed)
    agent.load(model_directory)
    env = RunenkriegEnv(seed ^ 0x5157, rule_sign=-1.0)
    rng = random.Random(seed ^ 0x5157)
    rows: list[dict] = []
    trained = 0
    for checkpoint in checkpoints:
        while trained < checkpoint:
            run_round(agent, env, True, rng)
            trained += 1
        metrics = evaluate(
            agent,
            range(40_000, 40_000 + evaluation_games),
            rule_sign=-1.0,
        )
        rows.append(
            {
                "agent": agent_name,
                "seed": seed,
                "rule_change_rounds": checkpoint,
                **metrics,
            }
        )
        env.reset()
        agent.reset_episode()
    return rows


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def completed_rows(
    output: Path,
    agent_name: str,
    seed: int,
    checkpoints: tuple[int, ...],
) -> list[dict] | None:
    rows: list[dict] = []
    for checkpoint in checkpoints:
        metrics = (
            output
            / "models"
            / f"seed_{seed}"
            / agent_name
            / f"round_{checkpoint}"
            / "metrics.json"
        )
        if not metrics.exists():
            return None
        rows.append(json.loads(metrics.read_text(encoding="utf-8")))
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description="Fair Runenkrieg TensorFlow benchmark")
    parser.add_argument("--agents", nargs="+", default=list(AGENTS), choices=AGENTS)
    parser.add_argument(
        "--seeds",
        nargs="+",
        type=int,
        default=list(TRAINING_SEEDS),
        help="pre-registered independent training seeds",
    )
    parser.add_argument("--output", type=Path, default=Path("results"))
    parser.add_argument("--quick", action="store_true", help="use 25/50 rounds for a smoke run")
    parser.add_argument(
        "--resume",
        action="store_true",
        help="reuse only fully completed agent/seed runs",
    )
    args = parser.parse_args()
    checkpoints = (25, 50) if args.quick else CHECKPOINTS
    evaluation_games = 2 if args.quick else 20
    args.output.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    adaptation_rows: list[dict] = []
    for agent_name in args.agents:
        for seed in args.seeds:
            existing = (
                completed_rows(args.output, agent_name, seed, checkpoints)
                if args.resume
                else None
            )
            if existing is None:
                seed_rows = train_agent(
                    agent_name,
                    seed,
                    args.output,
                    checkpoints,
                    evaluation_games,
                )
            else:
                seed_rows = existing
                print(f"RESUME {agent_name} seed={seed}", flush=True)
            rows.extend(seed_rows)
            adaptation_path = (
                args.output
                / "adaptation"
                / f"seed_{seed}"
                / f"{agent_name}.json"
            )
            if args.resume and adaptation_path.exists():
                seed_adaptation = json.loads(adaptation_path.read_text(encoding="utf-8"))
            else:
                seed_adaptation = rule_change_adaptation(
                    agent_name,
                    seed,
                    args.output
                    / "models"
                    / f"seed_{seed}"
                    / agent_name
                    / f"round_{checkpoints[-1]}",
                    args.output,
                    evaluation_games,
                    checkpoints=(0, 25, 50) if args.quick else (0, 250, 500, 1_000),
                )
                adaptation_path.parent.mkdir(parents=True, exist_ok=True)
                adaptation_path.write_text(
                    json.dumps(seed_adaptation, indent=2),
                    encoding="utf-8",
                )
            adaptation_rows.extend(seed_adaptation)
            write_csv(args.output / "learning_curves.csv", rows)
            write_csv(args.output / "rule_change_adaptation.csv", adaptation_rows)
    (args.output / "manifest.json").write_text(
        json.dumps(
            {
                "protocol": "RUNENKRIEG-TF-MULTISEED-1",
                "training_seeds": args.seeds,
                "checkpoints": checkpoints,
                "agents": args.agents,
                "evaluation_games_per_checkpoint": evaluation_games,
                "holdout_seeds": [30_000, 30_000 + evaluation_games - 1],
                "rule_switch_seeds": [40_000, 40_000 + evaluation_games - 1],
                "winner_replication_seeds": [60_000, 60_049],
                "winner_rule": [
                    "highest mean game_win_rate at 10000 rounds",
                    "highest mean_token_swing",
                    "lowest mean decision_ms",
                    "lexicographic agent name",
                ],
                "python": platform.python_version(),
                "platform": platform.platform(),
                "processor": platform.processor(),
                "logical_cpu_count": os.cpu_count(),
                "tensorflow_deterministic_ops": os.environ.get(
                    "TF_DETERMINISTIC_OPS",
                    "",
                ),
                "tensorflow_onednn_opts": os.environ.get(
                    "TF_ENABLE_ONEDNN_OPTS",
                    "",
                ),
                "tensorflow": __import__("tensorflow").__version__,
            },
            indent=2,
        ),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
