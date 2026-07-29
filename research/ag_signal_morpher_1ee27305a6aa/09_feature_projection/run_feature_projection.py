"""Evaluate a causal four-feature projection for generated spike gates."""

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
        projection = NetworkConfig(
            neuron_count=16,
            seed=seed,
            plasticity_enabled=False,
            gate_timing=GateTiming.EMISSION_STATE,
            emission_feature=EmissionFeature.FEATURE_PROJECTION,
            projection_ei_weight=0.40,
            projection_slope_weight=0.25,
            projection_overshoot_weight=0.15,
            projection_isi_weight=0.20,
            membrane_slope_scale_mv_per_ms=1.0,
            threshold_overshoot_scale_mv=1.0,
            isi_tau_ms=50.0,
        )
        calibration_stimulus = assembly_stimulus(
            projection.neuron_count, 420, seed=seed
        )
        kernel_result = SpikingNetwork(projection).run(
            calibration_stimulus
        )
        empirical_gates = tuple(effective_gate_values(kernel_result))
        event_mean = float(kernel_result.metrics["effective_gate_mean"])
        ei_config = replace(
            projection,
            emission_feature=EmissionFeature.EI_BALANCE,
        )
        slope_config = replace(
            projection,
            projection_ei_weight=0.0,
            projection_slope_weight=1.0,
            projection_overshoot_weight=0.0,
            projection_isi_weight=0.0,
        )
        overshoot_config = replace(
            projection,
            projection_ei_weight=0.0,
            projection_slope_weight=0.0,
            projection_overshoot_weight=1.0,
            projection_isi_weight=0.0,
        )
        isi_config = replace(
            projection,
            projection_ei_weight=0.0,
            projection_slope_weight=0.0,
            projection_overshoot_weight=0.0,
            projection_isi_weight=1.0,
        )
        component_configs = {
            "ei": ei_config,
            "slope": slope_config,
            "overshoot": overshoot_config,
            "isi": isi_config,
        }
        component_results = {
            name: SpikingNetwork(config).run(calibration_stimulus)
            for name, config in component_configs.items()
        }
        variants = {
            "kernel_projection": projection,
            "kernel_ei_only": ei_config,
            "kernel_slope_only": slope_config,
            "kernel_overshoot_only": overshoot_config,
            "kernel_isi_only": isi_config,
            "event_constant": replace(
                projection,
                gate_mode=GateMode.CONSTANT,
                constant_gate=event_mean,
            ),
            "ei_event_constant": replace(
                ei_config,
                gate_mode=GateMode.CONSTANT,
                constant_gate=float(
                    component_results["ei"].metrics[
                        "effective_gate_mean"
                    ]
                ),
            ),
            "slope_event_constant": replace(
                slope_config,
                gate_mode=GateMode.CONSTANT,
                constant_gate=float(
                    component_results["slope"].metrics[
                        "effective_gate_mean"
                    ]
                ),
            ),
            "overshoot_event_constant": replace(
                overshoot_config,
                gate_mode=GateMode.CONSTANT,
                constant_gate=float(
                    component_results["overshoot"].metrics[
                        "effective_gate_mean"
                    ]
                ),
            ),
            "isi_event_constant": replace(
                isi_config,
                gate_mode=GateMode.CONSTANT,
                constant_gate=float(
                    component_results["isi"].metrics[
                        "effective_gate_mean"
                    ]
                ),
            ),
            "isi_distribution_random": replace(
                isi_config,
                gate_mode=GateMode.RANDOM,
                random_gate_values=tuple(
                    effective_gate_values(component_results["isi"])
                ),
            ),
            "sign_projection": replace(
                projection,
                gate_mode=GateMode.SIGN,
            ),
            "tanh_projection": replace(
                projection,
                gate_mode=GateMode.TANH,
            ),
            "distribution_random": replace(
                projection,
                gate_mode=GateMode.RANDOM,
                random_gate_values=empirical_gates,
            ),
            "time_shifted": replace(
                projection,
                gate_perturbation=GatePerturbation.TIME_SHIFTED,
            ),
            "state_shuffled": replace(
                projection,
                gate_perturbation=GatePerturbation.STATE_SHUFFLED,
            ),
            "disabled": replace(
                projection,
                gate_mode=GateMode.DISABLED,
            ),
        }
        for name, config in variants.items():
            result = (
                kernel_result
                if name == "kernel_projection"
                else (
                    component_results[name.removeprefix("kernel_").removesuffix("_only")]
                    if name in {
                        "kernel_ei_only",
                        "kernel_slope_only",
                        "kernel_overshoot_only",
                        "kernel_isi_only",
                    }
                    else SpikingNetwork(config).run(calibration_stimulus)
                )
            )
            dynamics_rows.append(
                {
                    "seed": seed,
                    "variant": name,
                    **result.metrics,
                    "voltage_rmse_vs_projection": trajectory_rmse(
                        result.voltages_mv, kernel_result.voltages_mv
                    ),
                    "spike_disagreement_vs_projection": sum(
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
            seed=9100 + seed,
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

    output_root = RESEARCH_ROOT / "09_feature_projection"
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
    variants = sorted({row["variant"] for row in classification_rows})
    for name in variants:
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
            "voltage_rmse_vs_projection_mean": statistics.fmean(
                float(row["voltage_rmse_vs_projection"])
                for row in dynamic_rows
            ),
            "spike_disagreement_vs_projection_total": sum(
                int(row["spike_disagreement_vs_projection"])
                for row in dynamic_rows
            ),
        }

    best_accuracy = max(
        values["accuracy_mean"] for values in aggregate.values()
    )
    projection_accuracy = aggregate["kernel_projection"]["accuracy_mean"]
    projection_efficiency = aggregate["kernel_projection"][
        "spikes_per_correct_decision_mean"
    ]
    payload = {
        "artifact": ARTIFACT,
        "status": "causal_feature_projection_characterized",
        "projection": {
            "formula": (
                "0.40*ei_balance + 0.25*membrane_slope + "
                "0.15*threshold_overshoot + 0.20*isi_state"
            ),
            "component_ranges": {
                "ei_balance": "[-1,1]",
                "membrane_slope": "tanh(dV/dt / 1 mV/ms)",
                "threshold_overshoot": "tanh((V-threshold) / 1 mV)",
                "isi_state": "2*exp(-ISI/50 ms)-1; first spike=-1",
            },
            "timing": "all components captured at threshold crossing",
        },
        "seeds": seeds,
        "controls": [
            "event_constant",
            "ei_event_constant",
            "slope_event_constant",
            "overshoot_event_constant",
            "isi_event_constant",
            "isi_distribution_random",
            "sign_projection",
            "tanh_projection",
            "distribution_random",
            "time_shifted",
            "state_shuffled",
            "disabled",
        ],
        "component_ablations": [
            "kernel_ei_only",
            "kernel_slope_only",
            "kernel_overshoot_only",
            "kernel_isi_only",
        ],
        "aggregate": aggregate,
        "best_accuracy": best_accuracy,
        "best_variants": [
            name
            for name, values in aggregate.items()
            if abs(values["accuracy_mean"] - best_accuracy) <= 1e-12
        ],
        "claims": {
            "projection_gate_is_dynamic": (
                aggregate["kernel_projection"][
                    "effective_gate_variance_mean"
                ] > 1e-6
                and aggregate["kernel_projection"][
                    "effective_gate_entropy_bits_mean"
                ] > 0.0
            ),
            "projection_beats_ei_only": (
                projection_accuracy
                > aggregate["kernel_ei_only"]["accuracy_mean"]
            ),
            "projection_beats_event_constant": (
                projection_accuracy
                > aggregate["event_constant"]["accuracy_mean"]
            ),
            "projection_beats_distribution_random": (
                projection_accuracy
                > aggregate["distribution_random"]["accuracy_mean"]
            ),
            "isi_beats_event_constant": (
                aggregate["kernel_isi_only"]["accuracy_mean"]
                > aggregate["isi_event_constant"]["accuracy_mean"]
            ),
            "isi_beats_distribution_random": (
                aggregate["kernel_isi_only"]["accuracy_mean"]
                > aggregate["isi_distribution_random"]["accuracy_mean"]
            ),
            "projection_is_most_spike_efficient": all(
                projection_efficiency
                <= values["spikes_per_correct_decision_mean"]
                for values in aggregate.values()
            ),
            "projection_beats_all_controls": (
                projection_accuracy == best_accuracy
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
    (output_root / "feature_projection_results.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
