from __future__ import annotations

from collections import deque
from dataclasses import dataclass
import random
from pathlib import Path
from typing import Sequence

import numpy as np
import tensorflow as tf


@dataclass
class Choice:
    index: int
    features: np.ndarray
    all_features: np.ndarray
    old_log_probability: float = 0.0


class Agent:
    name = "agent"

    def select(self, features: np.ndarray, training: bool, rng: random.Random) -> Choice:
        raise NotImplementedError

    def learn(self, choice: Choice, reward: float, next_features: np.ndarray, done: bool) -> None:
        raise NotImplementedError

    def reset_episode(self) -> None:
        pass

    def parameter_bytes(self) -> int:
        return 0

    def save(self, directory: Path) -> None:
        directory.mkdir(parents=True, exist_ok=True)

    def load(self, directory: Path) -> None:
        pass

    def history_sensitivity(self, features: np.ndarray) -> float:
        return 0.0


def _mlp(output_activation: str | None = None) -> tf.keras.Model:
    inputs = tf.keras.Input((128,), name="channels")
    hidden = tf.keras.layers.Dense(128, activation="relu")(inputs)
    hidden = tf.keras.layers.Dense(64, activation="relu")(hidden)
    output = tf.keras.layers.Dense(1, activation=output_activation)(hidden)
    return tf.keras.Model(inputs, output)


class MlpAgent(Agent):
    name = "mlp"

    def __init__(self, seed: int):
        tf.keras.utils.set_random_seed(seed)
        self.model = _mlp("sigmoid")
        self.model.compile(tf.keras.optimizers.Adam(3e-4), loss="mse")
        self.epsilon = 0.12

    def select(self, features: np.ndarray, training: bool, rng: random.Random) -> Choice:
        scores = self.model(features, training=False).numpy().reshape(-1)
        index = rng.randrange(len(features)) if training and rng.random() < self.epsilon else int(np.argmax(scores))
        return Choice(index, features[index], features)

    def learn(self, choice: Choice, reward: float, next_features: np.ndarray, done: bool) -> None:
        self.model.train_on_batch(choice.features[None, :], np.array([[reward]], np.float32))

    def parameter_bytes(self) -> int:
        return sum(weight.numpy().nbytes for weight in self.model.weights)

    def save(self, directory: Path) -> None:
        super().save(directory)
        self.model.save_weights(directory / "mlp.weights.h5")

    def load(self, directory: Path) -> None:
        self.model.load_weights(directory / "mlp.weights.h5")


class GruAgent(Agent):
    name = "gru"

    def __init__(self, seed: int, history_length: int = 8):
        tf.keras.utils.set_random_seed(seed)
        self.history_length = history_length
        inputs = tf.keras.Input((history_length, 128), name="sequence")
        hidden = tf.keras.layers.GRU(64)(inputs)
        hidden = tf.keras.layers.Dense(32, activation="relu")(hidden)
        output = tf.keras.layers.Dense(1, activation="sigmoid")(hidden)
        self.model = tf.keras.Model(inputs, output)
        self.model.compile(tf.keras.optimizers.Adam(3e-4), loss="mse")
        self.history: deque[np.ndarray] = deque(maxlen=history_length - 1)
        self.last_sequence: np.ndarray | None = None

    def _sequence(self, candidate: np.ndarray) -> np.ndarray:
        prefix = [np.zeros(128, np.float32)] * (self.history_length - 1 - len(self.history))
        return np.stack(prefix + list(self.history) + [candidate])

    def select(self, features: np.ndarray, training: bool, rng: random.Random) -> Choice:
        sequences = np.stack([self._sequence(candidate) for candidate in features])
        scores = self.model(sequences, training=False).numpy().reshape(-1)
        index = rng.randrange(len(features)) if training and rng.random() < 0.10 else int(np.argmax(scores))
        self.last_sequence = sequences[index]
        return Choice(index, features[index], features)

    def learn(self, choice: Choice, reward: float, next_features: np.ndarray, done: bool) -> None:
        assert self.last_sequence is not None
        self.model.train_on_batch(self.last_sequence[None, :], np.array([[reward]], np.float32))
        self.history.append(choice.features.copy())

    def reset_episode(self) -> None:
        self.history.clear()

    def parameter_bytes(self) -> int:
        return sum(weight.numpy().nbytes for weight in self.model.weights)

    def save(self, directory: Path) -> None:
        super().save(directory)
        self.model.save_weights(directory / "gru.weights.h5")

    def load(self, directory: Path) -> None:
        self.model.load_weights(directory / "gru.weights.h5")

    def history_sensitivity(self, features: np.ndarray) -> float:
        candidate = features[0]
        self.history.clear()
        without_history = float(self.model(self._sequence(candidate)[None, :], training=False)[0, 0])
        for index in range(self.history_length - 1):
            synthetic = np.zeros(128, np.float32)
            synthetic[(index * 17) % 127] = 1.0
            synthetic[127] = 1.0
            self.history.append(synthetic)
        with_history = float(self.model(self._sequence(candidate)[None, :], training=False)[0, 0])
        self.history.clear()
        return abs(with_history - without_history)


class DqnAgent(Agent):
    name = "dqn"

    def __init__(self, seed: int):
        tf.keras.utils.set_random_seed(seed)
        self.model = _mlp(None)
        self.target = _mlp(None)
        self.target.set_weights(self.model.get_weights())
        self.optimizer = tf.keras.optimizers.Adam(3e-4)
        self.replay: deque[tuple[np.ndarray, float, np.ndarray, bool]] = deque(maxlen=4096)
        self.rng = np.random.default_rng(seed)
        self.steps = 0

    @tf.function(reduce_retracing=True)
    def _apply_update(
        self,
        states: tf.Tensor,
        targets: tf.Tensor,
    ) -> tf.Tensor:
        with tf.GradientTape() as tape:
            prediction = tf.reshape(self.model(states, training=True), (-1,))
            loss = tf.reduce_mean(tf.square(prediction - targets))
        gradients = tape.gradient(loss, self.model.trainable_variables)
        self.optimizer.apply_gradients(zip(gradients, self.model.trainable_variables))
        return loss

    def select(self, features: np.ndarray, training: bool, rng: random.Random) -> Choice:
        scores = self.model(features, training=False).numpy().reshape(-1)
        epsilon = max(0.03, 0.20 / np.sqrt(1 + self.steps / 1000))
        index = rng.randrange(len(features)) if training and rng.random() < epsilon else int(np.argmax(scores))
        return Choice(index, features[index], features)

    def learn(self, choice: Choice, reward: float, next_features: np.ndarray, done: bool) -> None:
        self.replay.append((choice.features.copy(), reward, next_features.copy(), done))
        self.steps += 1
        if len(self.replay) < 64:
            return
        indices = self.rng.integers(0, len(self.replay), size=32)
        batch = [self.replay[int(index)] for index in indices]
        states = np.stack([item[0] for item in batch])
        active_candidates = [
            candidates
            for _, _, candidates, terminal in batch
            if not terminal and len(candidates) > 0
        ]
        future_values: list[float] = []
        if active_candidates:
            flattened = np.concatenate(active_candidates, axis=0)
            predictions = self.target(flattened, training=False).numpy().reshape(-1)
            offset = 0
            for candidates in active_candidates:
                end = offset + len(candidates)
                future_values.append(float(np.max(predictions[offset:end])))
                offset = end
        future_iterator = iter(future_values)
        targets = [
            observed_reward
            + 0.95
            * (
                next(future_iterator)
                if not terminal and len(candidates) > 0
                else 0.0
            )
            for _, observed_reward, candidates, terminal in batch
        ]
        self._apply_update(
            tf.convert_to_tensor(states, tf.float32),
            tf.convert_to_tensor(targets, tf.float32),
        )
        if self.steps % 100 == 0:
            self.target.set_weights(self.model.get_weights())

    def parameter_bytes(self) -> int:
        return sum(weight.numpy().nbytes for weight in self.model.weights) * 2

    def save(self, directory: Path) -> None:
        super().save(directory)
        self.model.save_weights(directory / "dqn.weights.h5")

    def load(self, directory: Path) -> None:
        self.model.load_weights(directory / "dqn.weights.h5")
        self.target.set_weights(self.model.get_weights())


class PpoAgent(Agent):
    name = "ppo"

    def __init__(self, seed: int):
        tf.keras.utils.set_random_seed(seed)
        self.model = _mlp(None)
        self.optimizer = tf.keras.optimizers.Adam(2.5e-4)
        self.baseline = 0.5

    @tf.function(reduce_retracing=True)
    def _apply_update(
        self,
        features: tf.Tensor,
        choice_index: tf.Tensor,
        old_log_probability: tf.Tensor,
        advantage: tf.Tensor,
    ) -> tf.Tensor:
        with tf.GradientTape() as tape:
            logits = tf.reshape(self.model(features, training=True), (-1,))
            log_probability = tf.nn.log_softmax(logits)[choice_index]
            ratio = tf.exp(log_probability - old_log_probability)
            unclipped = ratio * advantage
            clipped = tf.clip_by_value(ratio, 0.8, 1.2) * advantage
            entropy = -tf.reduce_sum(
                tf.nn.softmax(logits) * tf.nn.log_softmax(logits)
            )
            loss = -tf.minimum(unclipped, clipped) - 0.01 * entropy
        gradients = tape.gradient(loss, self.model.trainable_variables)
        self.optimizer.apply_gradients(zip(gradients, self.model.trainable_variables))
        return loss

    def select(self, features: np.ndarray, training: bool, rng: random.Random) -> Choice:
        logits = tf.reshape(self.model(features, training=False), (-1,))
        probabilities = tf.nn.softmax(logits).numpy()
        if training:
            index = int(rng.choices(range(len(features)), weights=probabilities, k=1)[0])
        else:
            index = int(np.argmax(probabilities))
        return Choice(index, features[index], features, float(np.log(probabilities[index] + 1e-8)))

    def learn(self, choice: Choice, reward: float, next_features: np.ndarray, done: bool) -> None:
        advantage = float(reward - self.baseline)
        self.baseline = 0.98 * self.baseline + 0.02 * reward
        self._apply_update(
            tf.convert_to_tensor(choice.all_features, tf.float32),
            tf.convert_to_tensor(choice.index, tf.int32),
            tf.convert_to_tensor(choice.old_log_probability, tf.float32),
            tf.convert_to_tensor(advantage, tf.float32),
        )

    def parameter_bytes(self) -> int:
        return sum(weight.numpy().nbytes for weight in self.model.weights)

    def save(self, directory: Path) -> None:
        super().save(directory)
        self.model.save_weights(directory / "ppo.weights.h5")

    def load(self, directory: Path) -> None:
        self.model.load_weights(directory / "ppo.weights.h5")


class ContextualBanditAgent(Agent):
    name = "contextual_bandit"

    def __init__(self, seed: int):
        self.weights = np.zeros(128, np.float32)
        self.count = 0

    def select(self, features: np.ndarray, training: bool, rng: random.Random) -> Choice:
        scores = features @ self.weights
        epsilon = max(0.03, 0.18 / np.sqrt(1 + self.count / 1000))
        index = rng.randrange(len(features)) if training and rng.random() < epsilon else int(np.argmax(scores))
        return Choice(index, features[index], features)

    def learn(self, choice: Choice, reward: float, next_features: np.ndarray, done: bool) -> None:
        prediction = float(choice.features @ self.weights)
        self.weights += 0.03 * (reward - prediction) * choice.features
        self.weights = np.clip(self.weights, -3, 3)
        self.count += 1

    def parameter_bytes(self) -> int:
        return self.weights.nbytes

    def save(self, directory: Path) -> None:
        super().save(directory)
        np.savez_compressed(directory / "contextual_bandit.npz", weights=self.weights, count=self.count)

    def load(self, directory: Path) -> None:
        data = np.load(directory / "contextual_bandit.npz")
        self.weights[:] = data["weights"]
        self.count = int(data["count"])


def create_agent(name: str, seed: int) -> Agent:
    constructors = {
        "mlp": MlpAgent,
        "gru": GruAgent,
        "dqn": DqnAgent,
        "ppo": PpoAgent,
        "contextual_bandit": ContextualBanditAgent,
    }
    if name not in constructors:
        raise ValueError(f"Unknown agent {name!r}; choose {sorted(constructors)}")
    return constructors[name](seed)
