"""Deterministic current-based LIF network for synthetic research assays."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
import math
import random
from typing import Iterable, Sequence

from .kernel import synaptic_gate


class GateMode(str, Enum):
    """Ablation-selectable synaptic gate behavior."""

    KERNEL = "kernel"
    CONSTANT = "constant"
    DISABLED = "disabled"
    SIGN = "sign"
    TANH = "tanh"
    RANDOM = "random"


class GateTiming(str, Enum):
    """Temporal position at which a gate becomes bound to a spike."""

    RESET_LOCKED = "reset_locked"
    EMISSION_STATE = "emission_state"


class GatePerturbation(str, Enum):
    """Causal controls for gate timing and neuron assignment."""

    NONE = "none"
    TIME_SHIFTED = "time_shifted"
    STATE_SHUFFLED = "state_shuffled"


class EmissionFeature(str, Enum):
    """Scalar event state presented to the generated kernel at spike time."""

    PRE_RESET_VOLTAGE = "pre_reset_voltage"
    EI_BALANCE = "ei_balance"
    FEATURE_PROJECTION = "feature_projection"


@dataclass(frozen=True)
class SpikeEvent:
    """Causal spike payload retained independently of membrane reset."""

    source_neuron: int
    emission_step: int
    amplitude: float
    generated_gate: float
    feature_value: float
    ei_balance: float
    membrane_slope: float
    threshold_overshoot: float
    isi_state: float


@dataclass(frozen=True)
class NetworkConfig:
    neuron_count: int = 24
    excitatory_fraction: float = 0.8
    connection_probability: float = 0.18
    seed: int = 38
    dt_ms: float = 1.0
    tau_membrane_ms: float = 20.0
    tau_synapse_ms: float = 5.0
    resting_mv: float = -65.0
    reset_mv: float = -70.0
    threshold_mv: float = -50.0
    refractory_ms: float = 2.0
    excitatory_weight: float = 13.0
    inhibitory_weight: float = 17.0
    maximum_weight: float = 30.0
    adaptive_threshold_increment_mv: float = 1.5
    adaptive_threshold_tau_ms: float = 80.0
    gate_mode: GateMode = GateMode.KERNEL
    gate_timing: GateTiming = GateTiming.RESET_LOCKED
    gate_perturbation: GatePerturbation = GatePerturbation.NONE
    emission_feature: EmissionFeature = EmissionFeature.PRE_RESET_VOLTAGE
    projection_ei_weight: float = 0.40
    projection_slope_weight: float = 0.25
    projection_overshoot_weight: float = 0.15
    projection_isi_weight: float = 0.20
    membrane_slope_scale_mv_per_ms: float = 1.0
    threshold_overshoot_scale_mv: float = 1.0
    isi_tau_ms: float = 50.0
    gate_input_scale: float = 1.0
    constant_gate: float = 0.12831112128784755
    random_gate_amplitude: float = 0.35
    random_gate_values: tuple[float, ...] = ()
    plasticity_enabled: bool = True
    stdp_learning_rate: float = 0.01
    stdp_tau_ms: float = 20.0
    stdp_potentiation: float = 1.0
    stdp_depression: float = 1.05

    def __post_init__(self) -> None:
        if self.neuron_count <= 0:
            raise ValueError("neuron_count must be positive")
        if not 0.0 < self.excitatory_fraction <= 1.0:
            raise ValueError("excitatory_fraction must be in (0, 1]")
        if not 0.0 <= self.connection_probability <= 1.0:
            raise ValueError("connection_probability must be in [0, 1]")
        if self.dt_ms <= 0.0:
            raise ValueError("dt_ms must be positive")
        if self.tau_membrane_ms <= 0.0 or self.tau_synapse_ms <= 0.0:
            raise ValueError("time constants must be positive")
        if self.threshold_mv <= self.resting_mv:
            raise ValueError("threshold_mv must exceed resting_mv")
        if self.reset_mv > self.resting_mv:
            raise ValueError("reset_mv must not exceed resting_mv")
        if not 0.0 <= self.constant_gate <= 1.0:
            raise ValueError("constant_gate must be in [0, 1]")
        if not 0.0 <= self.random_gate_amplitude <= 1.0:
            raise ValueError("random_gate_amplitude must be in [0, 1]")
        projection_values = (
            self.projection_ei_weight,
            self.projection_slope_weight,
            self.projection_overshoot_weight,
            self.projection_isi_weight,
        )
        if not all(math.isfinite(value) for value in projection_values):
            raise ValueError("projection weights must be finite")
        if (
            not math.isfinite(self.membrane_slope_scale_mv_per_ms)
            or self.membrane_slope_scale_mv_per_ms <= 0.0
            or not math.isfinite(self.threshold_overshoot_scale_mv)
            or self.threshold_overshoot_scale_mv <= 0.0
            or not math.isfinite(self.isi_tau_ms)
            or self.isi_tau_ms <= 0.0
        ):
            raise ValueError("feature scales and isi_tau_ms must be positive")
        if any(
            not math.isfinite(value) or not 0.0 <= value <= 1.0
            for value in self.random_gate_values
        ):
            raise ValueError("random_gate_values must be finite values in [0, 1]")
        if self.maximum_weight <= 0.0:
            raise ValueError("maximum_weight must be positive")


@dataclass
class SimulationResult:
    """Time-major traces and aggregate metrics from one simulation."""

    voltages_mv: list[list[float]]
    spikes: list[list[int]]
    gates: list[list[float]]
    computed_gates: list[list[float]]
    events: list[SpikeEvent]
    final_weights: list[list[float]]
    dt_ms: float
    resting_mv: float
    threshold_mv: float
    metrics: dict[str, float | int | bool] = field(default_factory=dict)

    def compute_metrics(self) -> dict[str, float | int | bool]:
        neuron_count = len(self.spikes[0]) if self.spikes else len(self.final_weights)
        steps = len(self.spikes)
        total_spikes = sum(sum(row) for row in self.spikes)
        duration_s = max(self.dt_ms * steps / 1000.0, 1e-12)
        firing_rate = total_spikes / max(1, neuron_count) / duration_s
        scale = max(self.threshold_mv - self.resting_mv, 1e-12)
        voltage_values = [
            (value - self.resting_mv) / scale
            for row in self.voltages_mv
            for value in row
        ]
        voltage_energy = (
            sum(value * value for value in voltage_values) / len(voltage_values)
            if voltage_values
            else 0.0
        )
        gate_values = [value for row in self.gates for value in row]
        mean_gate = sum(gate_values) / len(gate_values) if gate_values else 1.0
        gate_variance = (
            sum((value - mean_gate) ** 2 for value in gate_values)
            / len(gate_values)
            if gate_values
            else 0.0
        )
        population_counts = [sum(row) for row in self.spikes]
        mean_population = (
            sum(population_counts) / len(population_counts)
            if population_counts
            else 0.0
        )
        population_spike_count_fano = (
            sum((value - mean_population) ** 2 for value in population_counts)
            / len(population_counts)
            / (mean_population + 1e-9)
            if population_counts
            else 0.0
        )
        effective_values = [
            self.gates[step][neuron]
            for step in range(1, steps)
            for neuron in range(neuron_count)
            if self.spikes[step - 1][neuron]
        ]
        effective_mean = (
            sum(effective_values) / len(effective_values)
            if effective_values
            else 0.0
        )
        effective_variance = (
            sum((value - effective_mean) ** 2 for value in effective_values)
            / len(effective_values)
            if effective_values
            else 0.0
        )
        if (
            len(effective_values) < 2
            or effective_variance <= 1e-15
            or max(effective_values) - min(effective_values) <= 1e-12
        ):
            effective_entropy = 0.0
        else:
            bin_count = min(16, max(2, round(math.sqrt(len(effective_values)))))
            lower = min(effective_values)
            width = (max(effective_values) - lower) / bin_count
            counts = [0] * bin_count
            for value in effective_values:
                index = min(
                    bin_count - 1,
                    int((value - lower) / width),
                )
                counts[index] += 1
            effective_entropy = -sum(
                (count / len(effective_values))
                * math.log2(count / len(effective_values))
                for count in counts
                if count
            )
        event_features = [event.feature_value for event in self.events]
        event_feature_mean = (
            sum(event_features) / len(event_features)
            if event_features
            else 0.0
        )
        computed_values = [
            value for row in self.computed_gates for value in row
        ]
        computed_mean = (
            sum(computed_values) / len(computed_values)
            if computed_values
            else 1.0
        )
        computed_variance = (
            sum((value - computed_mean) ** 2 for value in computed_values)
            / len(computed_values)
            if computed_values
            else 0.0
        )
        pairwise_correlations = []
        if steps > 1:
            for left in range(neuron_count):
                left_values = [row[left] for row in self.spikes]
                left_mean = sum(left_values) / steps
                left_variance = sum(
                    (value - left_mean) ** 2 for value in left_values
                )
                if left_variance <= 0.0:
                    continue
                for right in range(left + 1, neuron_count):
                    right_values = [row[right] for row in self.spikes]
                    right_mean = sum(right_values) / steps
                    right_variance = sum(
                        (value - right_mean) ** 2 for value in right_values
                    )
                    if right_variance <= 0.0:
                        continue
                    covariance = sum(
                        (a - left_mean) * (b - right_mean)
                        for a, b in zip(left_values, right_values)
                    )
                    pairwise_correlations.append(
                        covariance / math.sqrt(left_variance * right_variance)
                    )
        coincident_spikes = sum(
            count for count in population_counts if count > 1
        )
        active_neurons = sum(
            any(row[index] for row in self.spikes)
            for index in range(neuron_count)
        )
        finite = all(
            math.isfinite(value)
            for row in self.voltages_mv
            for value in row
        )

        def component_metrics(
            name: str, values: list[float]
        ) -> dict[str, float]:
            component_mean = (
                sum(values) / len(values) if values else 0.0
            )
            return {
                f"{name}_mean": component_mean,
                f"{name}_variance": (
                    sum(
                        (value - component_mean) ** 2
                        for value in values
                    )
                    / len(values)
                    if values
                    else 0.0
                ),
            }

        self.metrics = {
            "steps": steps,
            "neuron_count": neuron_count,
            "total_spikes": total_spikes,
            "mean_firing_rate_hz": firing_rate,
            "normalized_voltage_energy": voltage_energy,
            "mean_gate": mean_gate,
            "gate_variance": gate_variance,
            "population_spike_count_fano": population_spike_count_fano,
            "mean_pairwise_spike_correlation": (
                sum(pairwise_correlations) / len(pairwise_correlations)
                if pairwise_correlations
                else 0.0
            ),
            "pairwise_correlation_pairs": len(pairwise_correlations),
            "binned_coincidence_rate": (
                coincident_spikes / total_spikes if total_spikes else 0.0
            ),
            "active_fraction": active_neurons / max(1, neuron_count),
            "globally_computed_gate_mean": computed_mean,
            "globally_computed_gate_variance": computed_variance,
            "effective_gate_count": len(effective_values),
            "effective_gate_mean": effective_mean,
            "effective_gate_variance": effective_variance,
            "effective_gate_entropy_bits": effective_entropy,
            "effective_gate_minimum": (
                min(effective_values) if effective_values else 0.0
            ),
            "effective_gate_maximum": (
                max(effective_values) if effective_values else 0.0
            ),
            "spike_event_count": len(self.events),
            "event_feature_mean": event_feature_mean,
            "event_feature_variance": (
                sum(
                    (value - event_feature_mean) ** 2
                    for value in event_features
                )
                / len(event_features)
                if event_features
                else 0.0
            ),
            "event_feature_minimum": (
                min(event_features) if event_features else 0.0
            ),
            "event_feature_maximum": (
                max(event_features) if event_features else 0.0
            ),
            **component_metrics(
                "event_ei_balance",
                [event.ei_balance for event in self.events],
            ),
            **component_metrics(
                "event_membrane_slope",
                [event.membrane_slope for event in self.events],
            ),
            **component_metrics(
                "event_threshold_overshoot",
                [event.threshold_overshoot for event in self.events],
            ),
            **component_metrics(
                "event_isi_state",
                [event.isi_state for event in self.events],
            ),
            "finite": finite,
        }
        return dict(self.metrics)


class SpikingNetwork:
    """Sparse LIF network with Dale-safe recurrence and optional pair STDP."""

    def __init__(self, config: NetworkConfig = NetworkConfig()) -> None:
        self.config = config
        self._rng = random.Random(config.seed)
        self.excitatory_count = max(
            1, min(config.neuron_count, round(
                config.neuron_count * config.excitatory_fraction
            ))
        )
        self.weights = self._create_weights()
        self.reset_state()

    def _create_weights(self) -> list[list[float]]:
        cfg = self.config
        weights = [
            [0.0 for _ in range(cfg.neuron_count)]
            for _ in range(cfg.neuron_count)
        ]
        for post in range(cfg.neuron_count):
            for pre in range(cfg.neuron_count):
                if pre == post or self._rng.random() > cfg.connection_probability:
                    continue
                if pre < self.excitatory_count:
                    magnitude = cfg.excitatory_weight * self._rng.uniform(0.75, 1.25)
                else:
                    magnitude = -cfg.inhibitory_weight * self._rng.uniform(0.75, 1.25)
                weights[post][pre] = magnitude
        return weights

    def reset_state(self) -> None:
        count = self.config.neuron_count
        self.voltage = [self.config.resting_mv] * count
        self.synaptic_state = [0.0] * count
        self.excitatory_synaptic_state = [0.0] * count
        self.inhibitory_synaptic_state = [0.0] * count
        self.refractory_steps = [0] * count
        self.previous_spikes = [0] * count
        self.pre_trace = [0.0] * count
        self.post_trace = [0.0] * count
        self.adaptive_threshold = [0.0] * count
        self.previous_emission_gates = [1.0] * count
        self.delayed_transmission_gates = [1.0] * count
        self.last_computed_gates = [1.0] * count
        self.last_spike_steps: list[int | None] = [None] * count
        self.events: list[SpikeEvent] = []
        self.step_index = 0

    def _kernel_inputs(self) -> list[float]:
        cfg = self.config
        voltage_scale = cfg.threshold_mv - cfg.resting_mv
        return [
            (value - cfg.resting_mv) / voltage_scale
            for value in self.voltage
        ]

    def _gate_for_input(self, value: float) -> float:
        cfg = self.config
        if cfg.gate_mode is GateMode.DISABLED:
            return 1.0
        if cfg.gate_mode is GateMode.CONSTANT:
            return cfg.constant_gate
        if cfg.gate_mode is GateMode.SIGN:
            return 0.9 if value >= 0.0 else 0.1
        if cfg.gate_mode is GateMode.TANH:
            return max(
                0.05,
                min(0.95, 0.5 * (1.0 + math.tanh(4.0 * value))),
            )
        if cfg.gate_mode is GateMode.RANDOM:
            if cfg.random_gate_values:
                return self._rng.choice(cfg.random_gate_values)
            return max(
                0.05,
                min(
                    0.95,
                    cfg.constant_gate
                    + self._rng.uniform(
                        -cfg.random_gate_amplitude,
                        cfg.random_gate_amplitude,
                    ),
                ),
            )
        return synaptic_gate(value, input_scale=cfg.gate_input_scale)

    def _gates_for_inputs(self, inputs: Sequence[float]) -> list[float]:
        return [self._gate_for_input(value) for value in inputs]

    def _gates(self) -> list[float]:
        return self._gates_for_inputs(self._kernel_inputs())

    def _apply_stdp(self, spikes: Sequence[int]) -> None:
        cfg = self.config
        decay = math.exp(-cfg.dt_ms / cfg.stdp_tau_ms)
        self.pre_trace = [value * decay for value in self.pre_trace]
        self.post_trace = [value * decay for value in self.post_trace]
        if cfg.plasticity_enabled:
            for post in range(cfg.neuron_count):
                for pre in range(cfg.neuron_count):
                    weight = self.weights[post][pre]
                    if weight == 0.0:
                        continue
                    delta_magnitude = cfg.stdp_learning_rate * (
                        cfg.stdp_potentiation
                        * spikes[post]
                        * self.pre_trace[pre]
                        - cfg.stdp_depression
                        * spikes[pre]
                        * self.post_trace[post]
                    )
                    sign = 1.0 if pre < self.excitatory_count else -1.0
                    magnitude = min(
                        cfg.maximum_weight,
                        max(0.0, abs(weight) + delta_magnitude),
                    )
                    self.weights[post][pre] = sign * magnitude
        self.pre_trace = [
            value + spike for value, spike in zip(self.pre_trace, spikes)
        ]
        self.post_trace = [
            value + spike for value, spike in zip(self.post_trace, spikes)
        ]

    def step(self, external_current: float | Sequence[float]) -> tuple[list[float], list[int], list[float]]:
        cfg = self.config
        if isinstance(external_current, (int, float)):
            drive = [float(external_current)] * cfg.neuron_count
        else:
            drive = [float(value) for value in external_current]
            if len(drive) != cfg.neuron_count:
                raise ValueError("external_current length must match neuron_count")
        if not all(math.isfinite(value) for value in drive):
            raise ValueError("external_current must contain only finite values")

        computed_gates = self._gates()
        if cfg.gate_timing is GateTiming.EMISSION_STATE:
            base_gates = list(self.previous_emission_gates)
        else:
            base_gates = list(computed_gates)
        if cfg.gate_perturbation is GatePerturbation.TIME_SHIFTED:
            gates = list(self.delayed_transmission_gates)
            self.delayed_transmission_gates = list(base_gates)
        elif cfg.gate_perturbation is GatePerturbation.STATE_SHUFFLED:
            gates = base_gates[1:] + base_gates[:1]
        else:
            gates = base_gates
        self.last_computed_gates = computed_gates
        synapse_decay = math.exp(-cfg.dt_ms / cfg.tau_synapse_ms)
        incoming_exc = [0.0] * cfg.neuron_count
        incoming_inh = [0.0] * cfg.neuron_count
        for post in range(cfg.neuron_count):
            for pre in range(cfg.neuron_count):
                contribution = (
                    self.weights[post][pre]
                    * self.previous_spikes[pre]
                    * gates[pre]
                )
                if contribution >= 0.0:
                    incoming_exc[post] += contribution
                else:
                    incoming_inh[post] += -contribution
            self.excitatory_synaptic_state[post] = (
                self.excitatory_synaptic_state[post] * synapse_decay
                + incoming_exc[post]
            )
            self.inhibitory_synaptic_state[post] = (
                self.inhibitory_synaptic_state[post] * synapse_decay
                + incoming_inh[post]
            )
            self.synaptic_state[post] = (
                self.excitatory_synaptic_state[post]
                - self.inhibitory_synaptic_state[post]
            )

        adaptation_decay = math.exp(
            -cfg.dt_ms / cfg.adaptive_threshold_tau_ms
        )
        spikes = [0] * cfg.neuron_count
        next_emission_gates = list(computed_gates)
        refractory_duration = max(1, math.ceil(cfg.refractory_ms / cfg.dt_ms))
        membrane_factor = cfg.dt_ms / cfg.tau_membrane_ms
        for neuron in range(cfg.neuron_count):
            self.adaptive_threshold[neuron] *= adaptation_decay
            if self.refractory_steps[neuron] > 0:
                self.refractory_steps[neuron] -= 1
                self.voltage[neuron] = cfg.reset_mv
                continue
            previous_voltage = self.voltage[neuron]
            dv = membrane_factor * (
                cfg.resting_mv
                - self.voltage[neuron]
                + drive[neuron]
                + self.synaptic_state[neuron]
            )
            self.voltage[neuron] += dv
            threshold = (
                cfg.threshold_mv + self.adaptive_threshold[neuron]
            )
            if self.voltage[neuron] >= threshold:
                spikes[neuron] = 1
                excitation = self.excitatory_synaptic_state[neuron]
                inhibition = self.inhibitory_synaptic_state[neuron]
                ei_balance = (
                    excitation - inhibition
                ) / (abs(excitation) + abs(inhibition) + 1e-9)
                membrane_slope = math.tanh(
                    (
                        (self.voltage[neuron] - previous_voltage)
                        / cfg.dt_ms
                    )
                    / cfg.membrane_slope_scale_mv_per_ms
                )
                threshold_overshoot = math.tanh(
                    (self.voltage[neuron] - threshold)
                    / cfg.threshold_overshoot_scale_mv
                )
                last_spike_step = self.last_spike_steps[neuron]
                if last_spike_step is None:
                    isi_state = -1.0
                else:
                    isi_ms = (
                        self.step_index - last_spike_step
                    ) * cfg.dt_ms
                    isi_state = (
                        2.0 * math.exp(-isi_ms / cfg.isi_tau_ms) - 1.0
                    )
                if cfg.emission_feature is EmissionFeature.EI_BALANCE:
                    event_feature = ei_balance
                elif (
                    cfg.emission_feature
                    is EmissionFeature.FEATURE_PROJECTION
                ):
                    event_feature = (
                        cfg.projection_ei_weight * ei_balance
                        + cfg.projection_slope_weight * membrane_slope
                        + cfg.projection_overshoot_weight
                        * threshold_overshoot
                        + cfg.projection_isi_weight * isi_state
                    )
                else:
                    event_feature = (
                        self.voltage[neuron] - cfg.resting_mv
                    ) / (cfg.threshold_mv - cfg.resting_mv)
                next_emission_gates[neuron] = self._gate_for_input(
                    event_feature
                )
                self.events.append(
                    SpikeEvent(
                        source_neuron=neuron,
                        emission_step=self.step_index,
                        amplitude=1.0,
                        generated_gate=next_emission_gates[neuron],
                        feature_value=event_feature,
                        ei_balance=ei_balance,
                        membrane_slope=membrane_slope,
                        threshold_overshoot=threshold_overshoot,
                        isi_state=isi_state,
                    )
                )
                self.last_spike_steps[neuron] = self.step_index
                self.voltage[neuron] = cfg.reset_mv
                self.refractory_steps[neuron] = refractory_duration
                self.adaptive_threshold[neuron] += (
                    cfg.adaptive_threshold_increment_mv
                )

        self._apply_stdp(spikes)
        self.previous_spikes = spikes
        self.previous_emission_gates = next_emission_gates
        self.step_index += 1
        return list(self.voltage), spikes, gates

    def run(
        self,
        stimulus: Iterable[float | Sequence[float]],
        *,
        reset_state: bool = True,
    ) -> SimulationResult:
        if reset_state:
            self.reset_state()
        voltages: list[list[float]] = []
        spikes: list[list[int]] = []
        gates: list[list[float]] = []
        computed_gates: list[list[float]] = []
        for external_current in stimulus:
            voltage_row, spike_row, gate_row = self.step(external_current)
            voltages.append(voltage_row)
            spikes.append(spike_row)
            gates.append(gate_row)
            computed_gates.append(list(self.last_computed_gates))
        result = SimulationResult(
            voltages_mv=voltages,
            spikes=spikes,
            gates=gates,
            computed_gates=computed_gates,
            events=list(self.events),
            final_weights=[row[:] for row in self.weights],
            dt_ms=self.config.dt_ms,
            resting_mv=self.config.resting_mv,
            threshold_mv=self.config.threshold_mv,
        )
        result.compute_metrics()
        return result

    def dale_principle_holds(self) -> bool:
        for row in self.weights:
            if any(value < 0.0 for value in row[:self.excitatory_count]):
                return False
            if any(value > 0.0 for value in row[self.excitatory_count:]):
                return False
        return True
