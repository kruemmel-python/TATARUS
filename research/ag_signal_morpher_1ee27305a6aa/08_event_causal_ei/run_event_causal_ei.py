"""Evaluate an event-causal generated gate driven by recurrent E/I balance."""

from __future__ import annotations

from dataclasses import replace
import csv
import json
import math
from pathlib import Path
import statistics
import sys

sys.dont_write_bytecode = True

ARTIFACT = "ag_signal_morpher_1ee27305a6aa"
PROJECT_ROOT = Path(__file__).resolve().parents[3]
RESEARCH_ROOT = PROJECT_ROOT / "research" / ARTIFACT
SOURCE_ROOT = RESEARCH_ROOT / "05_integration" / "source"
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
    assembly_stimulus,
)


def effective_gate_values(result) -> list[float]:
    return [
        result.gates[step][neuron]
        for step in range(1, len(result.spikes))
        for neuron in range(len(result.spikes[step]))
        if result.spikes[step - 1][neuron]
    ]


def trajectory_rmse(left, right) -> float:
    return math.sqrt(
        statistics.fmean(
            (a - b) ** 2
            for left_row, right_row in zip(left, right)
            for a, b in zip(left_row, right_row)
        )
    )


def main() -> None:
    seeds = [11, 23, 38, 53, 71]
    dynamics_rows: list[dict] = []
    classification_rows: list[dict] = []
    for seed in seeds:
        base = NetworkConfig(
            neuron_count=16,
            seed=seed,
            plasticity_enabled=False,
            gate_timing=GateTiming.EMISSION_STATE,
            emission_feature=EmissionFeature.EI_BALANCE,
        )
        calibration_stimulus = assembly_stimulus(
            base.neuron_count, 420, seed=seed
        )
        kernel_result = SpikingNetwork(base).run(calibration_stimulus)
        empirical_gates = tuple(effective_gate_values(kernel_result))
        event_mean = float(kernel_result.metrics["effective_gate_mean"])
        variants = {
            "kernel_ei": base,
            "event_constant": replace(
                base,
                gate_mode=GateMode.CONSTANT,
                constant_gate=event_mean,
            ),
            "sign": replace(base, gate_mode=GateMode.SIGN),
            "tanh": replace(base, gate_mode=GateMode.TANH),
            "distribution_random": replace(
                base,
                gate_mode=GateMode.RANDOM,
                random_gate_values=empirical_gates,
            ),
            "time_shifted": replace(
                base,
                gate_perturbation=GatePerturbation.TIME_SHIFTED,
            ),
            "state_shuffled": replace(
                base,
                gate_perturbation=GatePerturbation.STATE_SHUFFLED,
            ),
            "disabled": replace(base, gate_mode=GateMode.DISABLED),
        }
        for name, config in variants.items():
            result = (
                kernel_result
                if name == "kernel_ei"
                else SpikingNetwork(config).run(calibration_stimulus)
            )
            dynamics_rows.append(
                {
                    "seed": seed,
                    "variant": name,
                    **result.metrics,
                    "voltage_rmse_vs_kernel": trajectory_rmse(
                        result.voltages_mv, kernel_result.voltages_mv
                    ),
                    "spike_disagreement_vs_kernel": sum(
                        a != b
                        for left_row, right_row in zip(
                            result.spikes, kernel_result.spikes
                        )
                        for a, b in zip(left_row, right_row)
                    ),
                }
            )

        task = ClassificationConfig(
            samples_per_class=12,
            folds=3,
            steps=120,
            time_bins=4,
            pulse_current=2.0,
            noise_std=5.0,
            seed=8100 + seed,
        )
        for name, config in variants.items():
            evaluation = TemporalClassifier(config, task).evaluate(
                config.gate_mode
            )
            classification_rows.append(
                {
                    "seed": seed,
                    "variant": name,
                    "accuracy": evaluation.mean_accuracy,
                    "balanced_accuracy": (
                        evaluation.mean_balanced_accuracy
                    ),
                    "accuracy_stddev": evaluation.accuracy_stddev,
                    "mean_effective_gate": (
                        evaluation.mean_effective_gate
                    ),
                    "effective_gate_variance": (
                        evaluation.mean_effective_gate_variance
                    ),
                    "effective_gate_entropy_bits": (
                        evaluation.mean_effective_gate_entropy_bits
                    ),
                    "mean_spikes_per_sample": (
                        evaluation.mean_spikes_per_sample
                    ),
                    "spikes_per_correct_decision": (
                        evaluation.spikes_per_correct_decision
                    ),
                    "assembly_separation": statistics.fmean(
                        evaluation.fold_assembly_separations
                    ),
                }
            )

    output_root = RESEARCH_ROOT / "08_event_causal_ei"
    for filename, rows in (
        ("dynamics_ablation.csv", dynamics_rows),
        ("classification_ablation.csv", classification_rows),
    ):
        with (output_root / filename).open(
            "w", newline="", encoding="utf-8"
        ) as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)

    aggregate = {}
    for name in sorted({row["variant"] for row in classification_rows}):
        task_rows = [
            row for row in classification_rows if row["variant"] == name
        ]
        dynamic_rows = [
            row for row in dynamics_rows if row["variant"] == name
        ]
        aggregate[name] = {
            "accuracy_mean": statistics.fmean(
                float(row["accuracy"]) for row in task_rows
            ),
            "accuracy_stddev_across_seeds": statistics.pstdev(
                float(row["accuracy"]) for row in task_rows
            ),
            "effective_gate_variance_mean": statistics.fmean(
                float(row["effective_gate_variance"])
                for row in dynamic_rows
            ),
            "effective_gate_entropy_bits_mean": statistics.fmean(
                float(row["effective_gate_entropy_bits"])
                for row in dynamic_rows
            ),
            "spikes_per_correct_decision_mean": statistics.fmean(
                float(row["spikes_per_correct_decision"])
                for row in task_rows
            ),
            "assembly_separation_mean": statistics.fmean(
                float(row["assembly_separation"]) for row in task_rows
            ),
            "voltage_rmse_vs_kernel_mean": statistics.fmean(
                float(row["voltage_rmse_vs_kernel"])
                for row in dynamic_rows
            ),
            "spike_disagreement_vs_kernel_total": sum(
                int(row["spike_disagreement_vs_kernel"])
                for row in dynamic_rows
            ),
        }
    best_accuracy = max(
        values["accuracy_mean"] for values in aggregate.values()
    )
    payload = {
        "artifact": ARTIFACT,
        "status": "event_causal_dynamic_gate_characterized",
        "event": {
            "structure": (
                "source_neuron, emission_step, amplitude, generated_gate, "
                "feature_value"
            ),
            "feature": (
                "(I_exc-I_inh)/(abs(I_exc)+abs(I_inh)+1e-9)"
            ),
            "timing": "gate captured at threshold crossing before reset",
        },
        "seeds": seeds,
        "controls": [
            "event_constant",
            "sign",
            "tanh",
            "distribution_random",
            "time_shifted",
            "state_shuffled",
            "disabled",
        ],
        "aggregate": aggregate,
        "best_accuracy": best_accuracy,
        "best_variants": [
            name
            for name, values in aggregate.items()
            if abs(values["accuracy_mean"] - best_accuracy) <= 1e-12
        ],
        "claims": {
            "effective_gate_is_dynamic": (
                aggregate["kernel_ei"]["effective_gate_variance_mean"]
                > 1e-6
                and aggregate["kernel_ei"][
                    "effective_gate_entropy_bits_mean"
                ] > 0.0
            ),
            "kernel_beats_event_constant": (
                aggregate["kernel_ei"]["accuracy_mean"]
                > aggregate["event_constant"]["accuracy_mean"]
            ),
            "kernel_beats_all_controls": (
                aggregate["kernel_ei"]["accuracy_mean"]
                == best_accuracy
                and len(
                    [
                        name
                        for name, values in aggregate.items()
                        if values["accuracy_mean"] == best_accuracy
                    ]
                ) == 1
            ),
        },
        "decision": "KEEP_AND_RESEARCH",
    }
    (output_root / "event_causal_ei_results.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
