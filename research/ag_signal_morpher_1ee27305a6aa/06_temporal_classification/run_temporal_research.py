"""Run multi-seed temporal classification against strong gate baselines."""

from __future__ import annotations

from dataclasses import replace
import csv
import json
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
    GateMode,
    NetworkConfig,
    TemporalClassifier,
)


def main() -> None:
    seeds = [11, 23, 38, 53, 71]
    task_template = ClassificationConfig(
        samples_per_class=16,
        folds=4,
        steps=120,
        time_bins=4,
        baseline_current=12.5,
        pulse_current=2.0,
        noise_std=5.0,
        training_epochs=350,
    )
    rows: list[dict] = []
    for seed in seeds:
        network = NetworkConfig(
            neuron_count=16,
            seed=seed,
            constant_gate=0.12831112128784755,
            plasticity_enabled=True,
        )
        task = replace(task_template, seed=3801 + seed)
        kernel_classifier = TemporalClassifier(network, task)
        kernel_result = kernel_classifier.evaluate(GateMode.KERNEL)
        matched_gate = kernel_result.mean_effective_gate
        matched_network = replace(network, constant_gate=matched_gate)
        classifier = TemporalClassifier(matched_network, task)
        evaluations = [kernel_result] + [
            classifier.evaluate(mode)
            for mode in (
                GateMode.CONSTANT,
                GateMode.DISABLED,
                GateMode.SIGN,
                GateMode.TANH,
                GateMode.RANDOM,
            )
        ]
        for result in evaluations:
            rows.append(
                {
                    "seed": seed,
                    "gate_mode": result.gate_mode.value,
                    "matched_constant_gate": matched_gate,
                    "mean_accuracy": result.mean_accuracy,
                    "accuracy_stddev_across_folds": result.accuracy_stddev,
                    "balanced_accuracy": result.mean_balanced_accuracy,
                    "mean_firing_rate_hz": result.mean_firing_rate_hz,
                    "mean_gate": result.mean_gate,
                    "mean_effective_gate": result.mean_effective_gate,
                    "mean_effective_gate_variance": (
                        result.mean_effective_gate_variance
                    ),
                    "mean_effective_gate_entropy_bits": (
                        result.mean_effective_gate_entropy_bits
                    ),
                    "mean_spikes_per_sample": result.mean_spikes_per_sample,
                    "spikes_per_correct_decision": (
                        result.spikes_per_correct_decision
                    ),
                    "mean_assembly_separation": statistics.fmean(
                        result.fold_assembly_separations
                    ),
                    "true_0_pred_0": result.confusion_matrix[0][0],
                    "true_0_pred_1": result.confusion_matrix[0][1],
                    "true_1_pred_0": result.confusion_matrix[1][0],
                    "true_1_pred_1": result.confusion_matrix[1][1],
                }
            )

    csv_path = RESEARCH_ROOT / "06_temporal_classification" / "classification_results.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    aggregate = {}
    for mode in [member.value for member in GateMode]:
        selected = [row for row in rows if row["gate_mode"] == mode]
        accuracies = [float(row["mean_accuracy"]) for row in selected]
        balanced = [float(row["balanced_accuracy"]) for row in selected]
        rates = [float(row["mean_firing_rate_hz"]) for row in selected]
        aggregate[mode] = {
            "seed_count": len(selected),
            "accuracy_mean": statistics.fmean(accuracies),
            "accuracy_stddev_across_seeds": statistics.pstdev(accuracies),
            "accuracy_min": min(accuracies),
            "accuracy_max": max(accuracies),
            "balanced_accuracy_mean": statistics.fmean(balanced),
            "mean_firing_rate_hz": statistics.fmean(rates),
        }

    best_accuracy = max(
        item["accuracy_mean"] for item in aggregate.values()
    )
    tied_best = [
        mode
        for mode, item in aggregate.items()
        if abs(item["accuracy_mean"] - best_accuracy) <= 1e-12
    ]
    payload = {
        "artifact": ARTIFACT,
        "status": "experimentally_supported_on_synthetic_temporal_task",
        "task": {
            "description": (
                "Balanced two-class classification. Both classes contain the "
                "same two neural assemblies; only temporal order is reversed."
            ),
            "network_neurons": 16,
            "samples_per_class_per_seed": task_template.samples_per_class,
            "folds": task_template.folds,
            "steps": task_template.steps,
            "time_bins": task_template.time_bins,
            "pulse_current": task_template.pulse_current,
            "noise_std": task_template.noise_std,
            "features": "population firing rate for two assemblies in four time bins",
            "readout": "L2-regularized logistic regression fitted inside each fold",
            "data_leakage_control": (
                "standardization and readout fitting use training folds only"
            ),
        },
        "seeds": seeds,
        "controls": {
            "constant": (
                "fixed per seed to the kernel condition's event-conditioned "
                "effective gate mean"
            ),
            "disabled": "neutral multiplicative gate 1",
            "sign": "0.9 for non-negative state, else 0.1",
            "tanh": "clip((1+tanh(4z))/2,0.05,0.95)",
            "random": "seeded uniform variation around matched constant mean",
        },
        "aggregate": aggregate,
        "best_mean_accuracy": best_accuracy,
        "tied_best_modes": tied_best,
        "kernel_claim": {
            "outperforms_matched_constant_on_mean": (
                aggregate["kernel"]["accuracy_mean"]
                > aggregate["constant"]["accuracy_mean"]
            ),
            "outperforms_all_strong_baselines": (
                tied_best == ["kernel"]
            ),
            "interpretation": (
                "A positive result against damping controls does not establish "
                "uniqueness. Sign and tanh controls test whether the generated "
                "formula adds value beyond a generic polarity gate."
            ),
        },
    }
    json_path = RESEARCH_ROOT / "06_temporal_classification" / "classification_results.json"
    json_path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
