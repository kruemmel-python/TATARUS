from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import tensorflow as tf
import numpy as np

from .agents import ContextualBanditAgent, create_agent


def export_model(
    agent_name: str,
    checkpoint: Path,
    seed: int,
) -> tuple[bytes, float]:
    agent = create_agent(agent_name, seed)
    agent.load(checkpoint)
    if isinstance(agent, ContextualBanditAgent):
        inputs = tf.keras.Input((128,), name="channels")
        output = tf.keras.layers.Dense(1, use_bias=False, name="score")(inputs)
        model = tf.keras.Model(inputs, output)
        model.layers[-1].set_weights([agent.weights.reshape(128, 1)])
    elif agent_name == "gru":
        inputs = tf.keras.Input((8, 128), name="sequence")
        hidden = tf.keras.layers.GRU(64, unroll=True)(inputs)
        hidden = tf.keras.layers.Dense(32, activation="relu")(hidden)
        output = tf.keras.layers.Dense(1, activation="sigmoid")(hidden)
        model = tf.keras.Model(inputs, output)
        model.set_weights(agent.model.get_weights())
    else:
        model = agent.model
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    model_bytes = converter.convert()
    rng = np.random.default_rng(92017)
    shape = (3, 8, 128) if agent_name == "gru" else (3, 128)
    probe = rng.normal(0.0, 0.3, shape).astype(np.float32)
    expected = model(probe, training=False).numpy()
    lite = tf.lite.Interpreter(model_content=model_bytes)
    input_details = lite.get_input_details()[0]
    lite.resize_tensor_input(input_details["index"], probe.shape)
    lite.allocate_tensors()
    lite.set_tensor(lite.get_input_details()[0]["index"], probe)
    lite.invoke()
    actual = lite.get_tensor(lite.get_output_details()[0]["index"])
    max_error = float(np.max(np.abs(expected - actual)))
    if max_error > 0.02:
        raise RuntimeError(f"LiteRT export drift is too large: {max_error}")
    return model_bytes, max_error


def main() -> None:
    parser = argparse.ArgumentParser(description="Freeze the selected winner for Android")
    parser.add_argument("--winner", type=Path, default=Path("results_full/winner.json"))
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("exports/runenkrieg_frozen_winner.tflite"),
    )
    args = parser.parse_args()
    selection = json.loads(args.winner.read_text(encoding="utf-8"))
    agent_name = selection["winner_agent"]
    seed = int(selection["winner_training_seed"])
    checkpoint = Path(selection["checkpoint_directory"])
    model_bytes, verification_error = export_model(agent_name, checkpoint, seed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(model_bytes)
    metadata = {
        "protocol": selection["protocol"],
        "agent": agent_name,
        "training_seed": seed,
        "selection_checkpoint": selection["selection_checkpoint"],
        "input_channels": 128,
        "history_length": 8 if agent_name == "gru" else 1,
        "output": "one scalar score per legal candidate",
        "learning_on_android": False,
        "sha256": hashlib.sha256(model_bytes).hexdigest(),
        "bytes": len(model_bytes),
        "desktop_export_max_abs_error": verification_error,
        "requires_select_tf_ops": False,
        "independent_replication": selection["independent_replication"],
    }
    metadata_path = args.output.with_suffix(".json")
    metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(json.dumps(metadata, indent=2), flush=True)


if __name__ == "__main__":
    main()
