from __future__ import annotations

from dataclasses import replace
import importlib.util
import math
from pathlib import Path
import sys
import unittest

PROJECT_ROOT = Path(__file__).resolve().parents[4]
SOURCE_ROOT = PROJECT_ROOT / "research" / "ag_signal_morpher_1ee27305a6aa" / "05_integration" / "source"
sys.path.insert(0, str(SOURCE_ROOT))

from biological_neural_network import (  # noqa: E402
    ClassificationConfig,
    EmissionFeature,
    GateMode,
    GatePerturbation,
    GateTiming,
    NetworkConfig,
    SpikingNetwork,
    TemporalClassifier,
    TemporalDataset,
    assembly_stimulus,
    kernel,
    synaptic_gate,
)


class KernelSemanticsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        generated_path = (
            PROJECT_ROOT
            / "exports"
            / "generated"
            / "ag_signal_morpher_1ee27305a6aa_kernel.py"
        )
        spec = importlib.util.spec_from_file_location("original_kernel", generated_path)
        assert spec is not None and spec.loader is not None
        cls.original = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.original)

    def test_cpu_reference_matches_untouched_export(self) -> None:
        values = [
            -1_000_000.0,
            -10.0,
            -1.0,
            -1e-3,
            -1e-9,
            -1e-15,
            0.0,
            1e-15,
            1e-9,
            1e-3,
            1.0,
            10.0,
            1_000_000.0,
            float("nan"),
            float("inf"),
        ]
        for value in values:
            self.assertEqual(kernel(value), self.original.kernel(value))

    def test_gate_is_finite_bounded_and_state_dependent(self) -> None:
        low = synaptic_gate(-1.0)
        center = synaptic_gate(0.0)
        high = synaptic_gate(1.0)
        self.assertTrue(0.05 <= low < center < high <= 0.95)
        for value in [-math.inf, -1e6, 0.0, 1e6, math.inf, math.nan]:
            self.assertTrue(math.isfinite(synaptic_gate(value)))


class NetworkDynamicsTests(unittest.TestCase):
    def setUp(self) -> None:
        self.config = NetworkConfig(neuron_count=20, seed=73)
        self.stimulus = assembly_stimulus(20, 240, seed=73)

    def test_seeded_simulation_is_exactly_reproducible(self) -> None:
        first = SpikingNetwork(self.config).run(self.stimulus)
        second = SpikingNetwork(self.config).run(self.stimulus)
        self.assertEqual(first.spikes, second.spikes)
        self.assertEqual(first.voltages_mv, second.voltages_mv)
        self.assertEqual(first.final_weights, second.final_weights)

    def test_dale_principle_survives_stdp(self) -> None:
        network = SpikingNetwork(self.config)
        self.assertTrue(network.dale_principle_holds())
        result = network.run(self.stimulus)
        self.assertGreater(result.metrics["total_spikes"], 0)
        self.assertTrue(network.dale_principle_holds())
        self.assertTrue(result.metrics["finite"])

    def test_refractory_period_is_enforced(self) -> None:
        config = NetworkConfig(
            neuron_count=1,
            excitatory_fraction=1.0,
            connection_probability=0.0,
            refractory_ms=3.0,
            adaptive_threshold_increment_mv=0.0,
            gate_mode=GateMode.DISABLED,
            plasticity_enabled=False,
        )
        result = SpikingNetwork(config).run([80.0] * 120)
        spike_steps = [
            index for index, row in enumerate(result.spikes) if row[0]
        ]
        self.assertGreater(len(spike_steps), 2)
        for previous, current in zip(spike_steps, spike_steps[1:]):
            self.assertGreaterEqual(current - previous, 4)

    def test_kernel_differs_from_incorrect_global_mean_control(self) -> None:
        kernel_result = SpikingNetwork(self.config).run(self.stimulus)
        mean_gate = float(kernel_result.metrics["mean_gate"])
        constant_config = replace(
            self.config,
            gate_mode=GateMode.CONSTANT,
            constant_gate=mean_gate,
        )
        constant_result = SpikingNetwork(constant_config).run(self.stimulus)
        self.assertNotEqual(kernel_result.voltages_mv, constant_result.voltages_mv)
        squared_error = [
            (a - b) ** 2
            for kernel_row, constant_row in zip(
                kernel_result.voltages_mv, constant_result.voltages_mv
            )
            for a, b in zip(kernel_row, constant_row)
        ]
        rmse = math.sqrt(sum(squared_error) / len(squared_error))
        self.assertGreater(rmse, 0.01)

    def test_reset_locked_kernel_equals_event_matched_constant(self) -> None:
        config = replace(self.config, plasticity_enabled=False)
        kernel_result = SpikingNetwork(config).run(self.stimulus)
        effective_gate = float(kernel_result.metrics["effective_gate_mean"])
        self.assertAlmostEqual(effective_gate, 0.12831112128784755)
        self.assertEqual(kernel_result.metrics["effective_gate_variance"], 0.0)
        constant_result = SpikingNetwork(
            replace(
                config,
                gate_mode=GateMode.CONSTANT,
                constant_gate=effective_gate,
            )
        ).run(self.stimulus)
        self.assertEqual(kernel_result.spikes, constant_result.spikes)
        self.assertEqual(
            kernel_result.voltages_mv, constant_result.voltages_mv
        )

    def test_emission_timing_binds_pre_reset_gate(self) -> None:
        reset_result = SpikingNetwork(
            replace(
                self.config,
                plasticity_enabled=False,
                gate_timing=GateTiming.RESET_LOCKED,
            )
        ).run(self.stimulus)
        emission_result = SpikingNetwork(
            replace(
                self.config,
                plasticity_enabled=False,
                gate_timing=GateTiming.EMISSION_STATE,
            )
        ).run(self.stimulus)
        self.assertGreater(
            emission_result.metrics["effective_gate_mean"],
            reset_result.metrics["effective_gate_mean"],
        )
        self.assertNotEqual(reset_result.spikes, emission_result.spikes)

    def test_gate_timing_controls_are_deterministic(self) -> None:
        for perturbation in (
            GatePerturbation.TIME_SHIFTED,
            GatePerturbation.STATE_SHUFFLED,
        ):
            config = replace(
                self.config,
                gate_perturbation=perturbation,
                plasticity_enabled=False,
            )
            first = SpikingNetwork(config).run(self.stimulus)
            second = SpikingNetwork(config).run(self.stimulus)
            self.assertEqual(first.spikes, second.spikes)
            self.assertEqual(first.gates, second.gates)

    def test_ei_balance_creates_dynamic_causal_spike_events(self) -> None:
        config = replace(
            self.config,
            gate_timing=GateTiming.EMISSION_STATE,
            emission_feature=EmissionFeature.EI_BALANCE,
            plasticity_enabled=False,
        )
        result = SpikingNetwork(config).run(self.stimulus)
        self.assertEqual(
            len(result.events), result.metrics["total_spikes"]
        )
        self.assertGreater(result.metrics["effective_gate_variance"], 1e-6)
        self.assertGreater(
            result.metrics["effective_gate_entropy_bits"], 0.0
        )
        self.assertTrue(
            any(event.feature_value < 0.0 for event in result.events)
        )
        self.assertTrue(
            any(event.feature_value > 0.0 for event in result.events)
        )

    def test_feature_projection_is_causal_and_reconstructable(self) -> None:
        config = replace(
            self.config,
            gate_timing=GateTiming.EMISSION_STATE,
            emission_feature=EmissionFeature.FEATURE_PROJECTION,
            projection_ei_weight=0.40,
            projection_slope_weight=0.25,
            projection_overshoot_weight=0.15,
            projection_isi_weight=0.20,
            plasticity_enabled=False,
        )
        result = SpikingNetwork(config).run(self.stimulus)
        self.assertGreater(len(result.events), 1)
        for event in result.events:
            reconstructed = (
                config.projection_ei_weight * event.ei_balance
                + config.projection_slope_weight
                * event.membrane_slope
                + config.projection_overshoot_weight
                * event.threshold_overshoot
                + config.projection_isi_weight * event.isi_state
            )
            self.assertAlmostEqual(event.feature_value, reconstructed)
            self.assertTrue(-1.0 <= event.ei_balance <= 1.0)
            self.assertTrue(-1.0 <= event.membrane_slope <= 1.0)
            self.assertTrue(0.0 <= event.threshold_overshoot <= 1.0)
            self.assertTrue(-1.0 <= event.isi_state <= 1.0)
        self.assertEqual(
            result.events[0].isi_state,
            -1.0,
        )
        self.assertGreater(result.metrics["effective_gate_variance"], 1e-6)
        self.assertGreater(
            result.metrics["event_membrane_slope_variance"], 0.0
        )
        self.assertGreater(
            result.metrics["event_threshold_overshoot_variance"], 0.0
        )
        self.assertGreater(result.metrics["event_isi_state_variance"], 0.0)

    def test_invalid_stimulus_is_rejected(self) -> None:
        network = SpikingNetwork(self.config)
        with self.assertRaises(ValueError):
            network.step([1.0, 2.0])
        with self.assertRaises(ValueError):
            network.step([float("nan")] * self.config.neuron_count)


class TemporalClassificationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.network = NetworkConfig(
            neuron_count=10,
            seed=91,
            constant_gate=0.8,
        )
        self.task = ClassificationConfig(
            samples_per_class=6,
            folds=2,
            steps=80,
            time_bins=4,
            training_epochs=80,
            seed=913,
        )

    def test_dataset_is_balanced_and_order_reversed(self) -> None:
        dataset = TemporalDataset.generate(self.network.neuron_count, self.task)
        labels = [sample.label for sample in dataset.samples]
        self.assertEqual(labels.count(0), self.task.samples_per_class)
        self.assertEqual(labels.count(1), self.task.samples_per_class)
        first_a = next(sample for sample in dataset.samples if sample.label == 0)
        first_b = next(sample for sample in dataset.samples if sample.label == 1)
        split = self.network.neuron_count // 2
        first_bin_a = sum(first_a.stimulus[0][:split])
        second_bin_a = sum(first_a.stimulus[0][split:])
        first_bin_b = sum(first_b.stimulus[0][:split])
        second_bin_b = sum(first_b.stimulus[0][split:])
        self.assertGreater(first_bin_a, second_bin_a)
        self.assertLess(first_bin_b, second_bin_b)

    def test_cross_validation_is_deterministic_and_complete(self) -> None:
        first = TemporalClassifier(self.network, self.task).evaluate(
            GateMode.KERNEL
        )
        second = TemporalClassifier(self.network, self.task).evaluate(
            GateMode.KERNEL
        )
        self.assertEqual(first.to_dict(), second.to_dict())
        self.assertEqual(first.feature_count, self.task.time_bins * 2)
        self.assertEqual(
            sum(sum(row) for row in first.confusion_matrix),
            self.task.samples_per_class * 2,
        )
        self.assertTrue(0.0 <= first.mean_accuracy <= 1.0)

    def test_all_gate_baselines_produce_valid_scores(self) -> None:
        classifier = TemporalClassifier(self.network, self.task)
        results = classifier.compare()
        self.assertEqual({item.gate_mode for item in results}, set(GateMode))
        for item in results:
            self.assertTrue(0.0 <= item.mean_accuracy <= 1.0)
            self.assertTrue(math.isfinite(item.mean_firing_rate_hz))


if __name__ == "__main__":
    unittest.main()
