"""Temporal-pattern datasets and a deterministic cross-validated readout."""

from __future__ import annotations

from dataclasses import dataclass, replace
import math
import random
import statistics

from .network import GateMode, NetworkConfig, SimulationResult, SpikingNetwork


@dataclass(frozen=True)
class ClassificationConfig:
    samples_per_class: int = 24
    folds: int = 4
    steps: int = 120
    time_bins: int = 4
    baseline_current: float = 12.5
    pulse_current: float = 2.0
    noise_std: float = 5.0
    seed: int = 3801
    learning_rate: float = 0.18
    training_epochs: int = 350
    l2: float = 0.002

    def __post_init__(self) -> None:
        if self.samples_per_class < 2:
            raise ValueError("samples_per_class must be at least 2")
        if not 2 <= self.folds <= self.samples_per_class:
            raise ValueError("folds must be between 2 and samples_per_class")
        if self.steps < self.time_bins or self.time_bins < 2:
            raise ValueError("steps must cover at least two time bins")
        if self.noise_std < 0.0:
            raise ValueError("noise_std must be non-negative")
        if self.training_epochs <= 0 or self.learning_rate <= 0.0:
            raise ValueError("readout training parameters must be positive")


@dataclass(frozen=True)
class TemporalSample:
    label: int
    sample_index: int
    stimulus: list[list[float]]


@dataclass
class TemporalDataset:
    samples: list[TemporalSample]
    description: str

    @classmethod
    def generate(
        cls,
        neuron_count: int,
        config: ClassificationConfig,
    ) -> "TemporalDataset":
        if neuron_count < 2:
            raise ValueError("temporal task needs at least two neurons")
        samples: list[TemporalSample] = []
        split = neuron_count // 2
        bin_width = config.steps / config.time_bins
        for label in (0, 1):
            for sample_index in range(config.samples_per_class):
                derived_seed = (
                    config.seed
                    ^ (label + 1) * 0x9E3779B1
                    ^ (sample_index + 1) * 0x85EBCA77
                )
                rng = random.Random(derived_seed)
                stimulus: list[list[float]] = []
                for step in range(config.steps):
                    time_bin = min(
                        config.time_bins - 1, int(step / bin_width)
                    )
                    assembly = (time_bin + label) % 2
                    row = []
                    for neuron in range(neuron_count):
                        in_first = neuron < split
                        active = in_first == (assembly == 0)
                        row.append(
                            config.baseline_current
                            + (config.pulse_current if active else 0.0)
                            + rng.gauss(0.0, config.noise_std)
                        )
                    stimulus.append(row)
                samples.append(
                    TemporalSample(
                        label=label,
                        sample_index=sample_index,
                        stimulus=stimulus,
                    )
                )
        return cls(
            samples=samples,
            description=(
                "Two balanced alternating assembly sequences with identical "
                "content and reversed temporal order."
            ),
        )


@dataclass
class GateEvaluation:
    gate_mode: GateMode
    fold_accuracies: list[float]
    fold_balanced_accuracies: list[float]
    confusion_matrix: list[list[int]]
    feature_count: int
    mean_gate: float
    mean_effective_gate: float
    mean_effective_gate_variance: float
    mean_effective_gate_entropy_bits: float
    mean_firing_rate_hz: float
    mean_spikes_per_sample: float
    spikes_per_correct_decision: float
    fold_assembly_separations: list[float]

    @property
    def mean_accuracy(self) -> float:
        return statistics.fmean(self.fold_accuracies)

    @property
    def accuracy_stddev(self) -> float:
        return (
            statistics.pstdev(self.fold_accuracies)
            if len(self.fold_accuracies) > 1
            else 0.0
        )

    @property
    def mean_balanced_accuracy(self) -> float:
        return statistics.fmean(self.fold_balanced_accuracies)

    def to_dict(self) -> dict:
        return {
            "gate_mode": self.gate_mode.value,
            "fold_accuracies": self.fold_accuracies,
            "fold_balanced_accuracies": self.fold_balanced_accuracies,
            "mean_accuracy": self.mean_accuracy,
            "accuracy_stddev": self.accuracy_stddev,
            "mean_balanced_accuracy": self.mean_balanced_accuracy,
            "confusion_matrix": self.confusion_matrix,
            "feature_count": self.feature_count,
            "mean_gate": self.mean_gate,
            "mean_effective_gate": self.mean_effective_gate,
            "mean_effective_gate_variance": (
                self.mean_effective_gate_variance
            ),
            "mean_effective_gate_entropy_bits": (
                self.mean_effective_gate_entropy_bits
            ),
            "mean_firing_rate_hz": self.mean_firing_rate_hz,
            "mean_spikes_per_sample": self.mean_spikes_per_sample,
            "spikes_per_correct_decision": (
                self.spikes_per_correct_decision
            ),
            "fold_assembly_separations": self.fold_assembly_separations,
            "mean_assembly_separation": statistics.fmean(
                self.fold_assembly_separations
            ),
        }


class TemporalClassifier:
    """Reservoir-style spike feature extraction plus logistic readout."""

    def __init__(
        self,
        network_config: NetworkConfig,
        task_config: ClassificationConfig = ClassificationConfig(),
    ) -> None:
        self.network_config = network_config
        self.task_config = task_config
        self.dataset = TemporalDataset.generate(
            network_config.neuron_count, task_config
        )

    def _features(self, result: SimulationResult) -> list[float]:
        bins = self.task_config.time_bins
        neuron_count = self.network_config.neuron_count
        split = max(1, neuron_count // 2)
        counts = [0.0] * (bins * 2)
        for step, spikes in enumerate(result.spikes):
            time_bin = min(
                bins - 1,
                step * bins // max(1, len(result.spikes)),
            )
            for neuron, spike in enumerate(spikes):
                assembly = 0 if neuron < split else 1
                counts[time_bin * 2 + assembly] += float(spike)
        bin_duration_s = (
            self.network_config.dt_ms
            * self.task_config.steps
            / bins
            / 1000.0
        )
        assembly_sizes = [split, neuron_count - split]
        return [
            value
            / max(
                bin_duration_s * assembly_sizes[index % 2],
                1e-12,
            )
            for index, value in enumerate(counts)
        ]

    def _extract_dataset(
        self, gate_mode: GateMode
    ) -> tuple[
        list[list[float]],
        list[int],
        float,
        float,
        float,
        float,
        float,
        float,
    ]:
        config = replace(self.network_config, gate_mode=gate_mode)
        features: list[list[float]] = []
        labels: list[int] = []
        gates: list[float] = []
        rates: list[float] = []
        effective_gates: list[float] = []
        effective_variances: list[float] = []
        effective_entropies: list[float] = []
        spike_totals: list[float] = []
        for sample in self.dataset.samples:
            result = SpikingNetwork(config).run(sample.stimulus)
            features.append(self._features(result))
            labels.append(sample.label)
            gates.append(float(result.metrics["mean_gate"]))
            rates.append(float(result.metrics["mean_firing_rate_hz"]))
            effective_gates.append(
                float(result.metrics["effective_gate_mean"])
            )
            effective_variances.append(
                float(result.metrics["effective_gate_variance"])
            )
            effective_entropies.append(
                float(result.metrics["effective_gate_entropy_bits"])
            )
            spike_totals.append(float(result.metrics["total_spikes"]))
        return (
            features,
            labels,
            statistics.fmean(gates),
            statistics.fmean(rates),
            statistics.fmean(effective_gates),
            statistics.fmean(effective_variances),
            statistics.fmean(effective_entropies),
            statistics.fmean(spike_totals),
        )

    @staticmethod
    def _standardize(
        train: list[list[float]],
        test: list[list[float]],
    ) -> tuple[list[list[float]], list[list[float]]]:
        dimensions = len(train[0])
        means = [
            statistics.fmean(row[column] for row in train)
            for column in range(dimensions)
        ]
        deviations = []
        for column, mean in enumerate(means):
            variance = statistics.fmean(
                (row[column] - mean) ** 2 for row in train
            )
            deviations.append(math.sqrt(variance) if variance > 1e-12 else 1.0)

        def transform(rows: list[list[float]]) -> list[list[float]]:
            return [
                [
                    (value - means[column]) / deviations[column]
                    for column, value in enumerate(row)
                ]
                for row in rows
            ]

        return transform(train), transform(test)

    def _train_readout(
        self, features: list[list[float]], labels: list[int]
    ) -> list[float]:
        dimensions = len(features[0])
        weights = [0.0] * (dimensions + 1)
        count = len(features)
        for _ in range(self.task_config.training_epochs):
            gradient = [0.0] * len(weights)
            for row, label in zip(features, labels):
                score = weights[-1] + sum(
                    weight * value
                    for weight, value in zip(weights[:-1], row)
                )
                score = max(-30.0, min(30.0, score))
                probability = 1.0 / (1.0 + math.exp(-score))
                error = probability - label
                for index, value in enumerate(row):
                    gradient[index] += error * value
                gradient[-1] += error
            for index in range(dimensions):
                gradient[index] = (
                    gradient[index] / count
                    + self.task_config.l2 * weights[index]
                )
            gradient[-1] /= count
            weights = [
                weight - self.task_config.learning_rate * change
                for weight, change in zip(weights, gradient)
            ]
        return weights

    @staticmethod
    def _predict(weights: list[float], row: list[float]) -> int:
        score = weights[-1] + sum(
            weight * value for weight, value in zip(weights[:-1], row)
        )
        return int(score >= 0.0)

    def evaluate(self, gate_mode: GateMode) -> GateEvaluation:
        (
            features,
            labels,
            mean_gate,
            mean_rate,
            mean_effective_gate,
            mean_effective_variance,
            mean_effective_entropy,
            mean_spikes,
        ) = self._extract_dataset(gate_mode)
        fold_accuracies: list[float] = []
        balanced: list[float] = []
        confusion = [[0, 0], [0, 0]]
        assembly_separations: list[float] = []
        total_correct = 0
        for fold in range(self.task_config.folds):
            test_indices = [
                index
                for index, sample in enumerate(self.dataset.samples)
                if sample.sample_index % self.task_config.folds == fold
            ]
            test_index_set = set(test_indices)
            train_indices = [
                index
                for index in range(len(features))
                if index not in test_index_set
            ]
            train_x = [features[index] for index in train_indices]
            train_y = [labels[index] for index in train_indices]
            test_x = [features[index] for index in test_indices]
            test_y = [labels[index] for index in test_indices]
            train_x, test_x = self._standardize(train_x, test_x)
            weights = self._train_readout(train_x, train_y)
            predictions = [self._predict(weights, row) for row in test_x]
            correct = sum(
                prediction == label
                for prediction, label in zip(predictions, test_y)
            )
            total_correct += correct
            fold_accuracies.append(correct / len(test_y))
            recalls = []
            for label in (0, 1):
                members = [
                    prediction == truth
                    for prediction, truth in zip(predictions, test_y)
                    if truth == label
                ]
                recalls.append(sum(members) / len(members))
            balanced.append(statistics.fmean(recalls))
            centroids = []
            for label in (0, 1):
                members = [
                    row
                    for row, truth in zip(test_x, test_y)
                    if truth == label
                ]
                centroids.append(
                    [
                        statistics.fmean(
                            row[column] for row in members
                        )
                        for column in range(len(test_x[0]))
                    ]
                )
            assembly_separations.append(
                math.sqrt(
                    sum(
                        (left - right) ** 2
                        for left, right in zip(
                            centroids[0], centroids[1]
                        )
                    )
                )
            )
            for truth, prediction in zip(test_y, predictions):
                confusion[truth][prediction] += 1
        return GateEvaluation(
            gate_mode=gate_mode,
            fold_accuracies=fold_accuracies,
            fold_balanced_accuracies=balanced,
            confusion_matrix=confusion,
            feature_count=len(features[0]),
            mean_gate=mean_gate,
            mean_effective_gate=mean_effective_gate,
            mean_effective_gate_variance=mean_effective_variance,
            mean_effective_gate_entropy_bits=mean_effective_entropy,
            mean_firing_rate_hz=mean_rate,
            mean_spikes_per_sample=mean_spikes,
            spikes_per_correct_decision=(
                mean_spikes * len(features) / max(1, total_correct)
            ),
            fold_assembly_separations=assembly_separations,
        )

    def compare(
        self,
        modes: tuple[GateMode, ...] = (
            GateMode.KERNEL,
            GateMode.CONSTANT,
            GateMode.DISABLED,
            GateMode.SIGN,
            GateMode.TANH,
            GateMode.RANDOM,
        ),
    ) -> list[GateEvaluation]:
        return [self.evaluate(mode) for mode in modes]
