"""Test effective gate values, reset locking, and emission-state timing."""

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
    GateMode,
    GatePerturbation,
    GateTiming,
    NetworkConfig,
    SpikingNetwork,
    assembly_stimulus,
)


def trajectory_rmse(left: list[list[float]], right: list[list[float]]) -> float:
    errors = [
        (a - b) ** 2
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    ]
    return math.sqrt(statistics.fmean(errors))


def run_variant(
    config: NetworkConfig,
    stimulus: list[list[float]],
    reference,
    seed: int,
    name: str,
) -> dict:
    result = SpikingNetwork(config).run(stimulus)
    return {
        "seed": seed,
        "variant": name,
        "gate_timing": config.gate_timing.value,
        "gate_mode": config.gate_mode.value,
        "gate_perturbation": config.gate_perturbation.value,
        **result.metrics,
        "voltage_rmse_vs_timing_reference": trajectory_rmse(
            result.voltages_mv, reference.voltages_mv
        ),
        "spike_disagreement_vs_timing_reference": sum(
            a != b
            for left_row, right_row in zip(result.spikes, reference.spikes)
            for a, b in zip(left_row, right_row)
        ),
    }


def main() -> None:
    seeds = [11, 23, 38, 53, 71]
    rows: list[dict] = []
    summaries = []
    for seed in seeds:
        base = NetworkConfig(
            neuron_count=24,
            seed=seed,
            plasticity_enabled=False,
        )
        stimulus = assembly_stimulus(
            base.neuron_count, 420, seed=seed
        )
        reset_config = replace(
            base, gate_timing=GateTiming.RESET_LOCKED
        )
        reset = SpikingNetwork(reset_config).run(stimulus)
        reset_event_mean = float(reset.metrics["effective_gate_mean"])
        reset_event_variance = float(
            reset.metrics["effective_gate_variance"]
        )
        reset_global_mean = float(
            reset.metrics["globally_computed_gate_mean"]
        )
        emission_config = replace(
            base, gate_timing=GateTiming.EMISSION_STATE
        )
        emission = SpikingNetwork(emission_config).run(stimulus)
        emission_event_mean = float(
            emission.metrics["effective_gate_mean"]
        )
        emission_event_variance = float(
            emission.metrics["effective_gate_variance"]
        )

        reset_variants = [
            ("reset_kernel", reset_config),
            (
                "reset_global_constant",
                replace(
                    reset_config,
                    gate_mode=GateMode.CONSTANT,
                    constant_gate=reset_global_mean,
                ),
            ),
            (
                "reset_event_constant",
                replace(
                    reset_config,
                    gate_mode=GateMode.CONSTANT,
                    constant_gate=reset_event_mean,
                ),
            ),
            (
                "reset_distribution_random",
                replace(
                    reset_config,
                    gate_mode=GateMode.RANDOM,
                    constant_gate=reset_event_mean,
                    random_gate_amplitude=math.sqrt(
                        3.0 * reset_event_variance
                    ),
                ),
            ),
            (
                "reset_time_shifted",
                replace(
                    reset_config,
                    gate_perturbation=GatePerturbation.TIME_SHIFTED,
                ),
            ),
            (
                "reset_state_shuffled",
                replace(
                    reset_config,
                    gate_perturbation=GatePerturbation.STATE_SHUFFLED,
                ),
            ),
        ]
        emission_variants = [
            ("emission_kernel", emission_config),
            (
                "emission_event_constant",
                replace(
                    emission_config,
                    gate_mode=GateMode.CONSTANT,
                    constant_gate=emission_event_mean,
                ),
            ),
            (
                "emission_distribution_random",
                replace(
                    emission_config,
                    gate_mode=GateMode.RANDOM,
                    constant_gate=emission_event_mean,
                    random_gate_amplitude=math.sqrt(
                        3.0 * emission_event_variance
                    ),
                ),
            ),
            (
                "emission_time_shifted",
                replace(
                    emission_config,
                    gate_perturbation=GatePerturbation.TIME_SHIFTED,
                ),
            ),
            (
                "emission_state_shuffled",
                replace(
                    emission_config,
                    gate_perturbation=GatePerturbation.STATE_SHUFFLED,
                ),
            ),
        ]
        for name, config in reset_variants:
            rows.append(
                run_variant(config, stimulus, reset, seed, name)
            )
        for name, config in emission_variants:
            rows.append(
                run_variant(config, stimulus, emission, seed, name)
            )
        summaries.append(
            {
                "seed": seed,
                "reset_effective_mean": reset_event_mean,
                "reset_effective_variance": reset_event_variance,
                "emission_effective_mean": emission_event_mean,
                "emission_effective_variance": emission_event_variance,
                "reset_vs_emission_voltage_rmse": trajectory_rmse(
                    reset.voltages_mv, emission.voltages_mv
                ),
                "reset_vs_emission_spike_disagreement": sum(
                    a != b
                    for left_row, right_row in zip(
                        reset.spikes, emission.spikes
                    )
                    for a, b in zip(left_row, right_row)
                ),
            }
        )

    csv_path = (
        RESEARCH_ROOT / "07_timing_ablation" / "timing_ablation.csv"
    )
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    def selected(variant: str) -> list[dict]:
        return [row for row in rows if row["variant"] == variant]

    reset_event_rows = selected("reset_event_constant")
    reset_shuffle_rows = selected("reset_state_shuffled")
    emission_event_rows = selected("emission_event_constant")
    payload = {
        "artifact": ARTIFACT,
        "status": "wrapper_timing_confounded",
        "seeds": seeds,
        "plasticity_enabled": False,
        "effective_gate_definition": (
            "g_j(t) conditioned on previous_spike_j(t-1)=1"
        ),
        "per_seed_summary": summaries,
        "acceptance_correction": {
            "kernel_gate_is_globally_state_dependent": True,
            "effective_transmission_gate_is_state_dependent": False,
            "kernel_differs_from_global_mean_constant": True,
            "kernel_differs_from_event_matched_constant": False,
            "wrapper_timing_confounded": True,
        },
        "reset_locked": {
            "effective_gate_mean": statistics.fmean(
                item["reset_effective_mean"] for item in summaries
            ),
            "effective_gate_variance_max": max(
                item["reset_effective_variance"] for item in summaries
            ),
            "event_constant_max_voltage_rmse": max(
                float(row["voltage_rmse_vs_timing_reference"])
                for row in reset_event_rows
            ),
            "event_constant_total_spike_disagreement": sum(
                int(row["spike_disagreement_vs_timing_reference"])
                for row in reset_event_rows
            ),
            "state_shuffled_max_voltage_rmse": max(
                float(row["voltage_rmse_vs_timing_reference"])
                for row in reset_shuffle_rows
            ),
            "interpretation": (
                "Every transmitted spike is multiplied by the gate evaluated "
                "at reset voltage; the phenotype is constant attenuation."
            ),
        },
        "emission_state": {
            "effective_gate_mean": statistics.fmean(
                item["emission_effective_mean"] for item in summaries
            ),
            "effective_gate_variance_max": max(
                item["emission_effective_variance"] for item in summaries
            ),
            "event_constant_max_voltage_rmse": max(
                float(row["voltage_rmse_vs_timing_reference"])
                for row in emission_event_rows
            ),
            "event_constant_total_spike_disagreement": sum(
                int(row["spike_disagreement_vs_timing_reference"])
                for row in emission_event_rows
            ),
            "interpretation": (
                "The gate is causally captured before reset, but normalized "
                "pre-reset voltages remain on the kernel's positive plateau."
            ),
        },
        "decision": "KEEP_AND_RESEARCH",
    }
    json_path = (
        RESEARCH_ROOT / "07_timing_ablation" / "timing_ablation.json"
    )
    json_path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
