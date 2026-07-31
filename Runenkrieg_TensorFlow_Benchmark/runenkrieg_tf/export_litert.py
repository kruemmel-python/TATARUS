from __future__ import annotations

import argparse
from pathlib import Path

import tensorflow as tf

from .agents import ContextualBanditAgent, create_agent


def main() -> None:
    parser = argparse.ArgumentParser(description="Export a trained baseline to LiteRT")
    parser.add_argument("--agent", required=True, choices=("mlp", "gru", "dqn", "ppo", "contextual_bandit"))
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20260730)
    args = parser.parse_args()
    agent = create_agent(args.agent, args.seed)
    agent.load(args.checkpoint)
    if isinstance(agent, ContextualBanditAgent):
        inputs = tf.keras.Input((128,))
        output = tf.keras.layers.Dense(1, use_bias=False)(inputs)
        model = tf.keras.Model(inputs, output)
        model.layers[-1].set_weights([agent.weights.reshape(128, 1)])
    else:
        model = agent.model
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    if args.agent == "gru":
        inputs = tf.keras.Input((8, 128), name="sequence")
        hidden = tf.keras.layers.GRU(64, unroll=True)(inputs)
        hidden = tf.keras.layers.Dense(32, activation="relu")(hidden)
        output = tf.keras.layers.Dense(1, activation="sigmoid")(hidden)
        unrolled = tf.keras.Model(inputs, output)
        unrolled.set_weights(model.get_weights())
        model = unrolled
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(converter.convert())
    print(f"{args.output} ({args.output.stat().st_size} Bytes)")


if __name__ == "__main__":
    main()
