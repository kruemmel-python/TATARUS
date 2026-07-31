from __future__ import annotations

import argparse
from pathlib import Path
import random

import numpy as np

from .agents import create_agent
from .encoding import encode
from .environment import RunenkriegEnv, Weather, legal_actions


def main() -> None:
    parser = argparse.ArgumentParser(description="Play Runenkrieg against a TensorFlow baseline")
    parser.add_argument("--agent", default="mlp", choices=("mlp", "gru", "dqn", "ppo", "contextual_bandit"))
    parser.add_argument("--weights", type=Path)
    parser.add_argument("--seed", type=int, default=20260730)
    args = parser.parse_args()
    agent = create_agent(args.agent, args.seed)
    if args.weights:
        if args.agent == "contextual_bandit":
            data = np.load(args.weights)
            agent.weights[:] = data["weights"]
            agent.count = int(data["count"])
        else:
            agent.model.load_weights(args.weights)
    env = RunenkriegEnv(args.seed)
    rng = random.Random(args.seed)
    while not env.state.done:
        state = env.state
        print(f"\nRunde {len(state.history) + 1} · Du {state.player_tokens} : KI {state.ai_tokens}")
        for index, card in enumerate(state.player_hand):
            print(f"  {index}: {card.element.name} Stärke {card.power} {card.card_type.name}")
        selected = int(input("Deine Karte: "))
        player = next(
            action
            for action in legal_actions(state.player_hand)
            if action.card.uid == state.player_hand[selected].uid
        )
        weather = env.rng.choice(tuple(Weather))
        actions = legal_actions(state.ai_hand)
        features = np.stack([encode(state, player.card, action, weather) for action in actions])
        choice = agent.select(features, False, rng)
        reward, winner = env.step(player, actions[choice.index], weather)
        print(
            f"KI spielt {actions[choice.index].card.element.name} "
            f"Stärke {actions[choice.index].card.power} · {winner.name} · Reward {reward:.2f}"
        )
    print(f"\nEndstand: Du {env.state.player_tokens} : KI {env.state.ai_tokens}")


if __name__ == "__main__":
    main()
