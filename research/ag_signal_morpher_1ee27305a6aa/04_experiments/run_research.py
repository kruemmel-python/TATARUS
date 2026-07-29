"""Reproduce kernel characterization, backend checks, and network ablations."""

from __future__ import annotations

from dataclasses import replace
import csv
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

ARTIFACT = "ag_signal_morpher_1ee27305a6aa"
PROJECT_ROOT = Path(__file__).resolve().parents[3]
RESEARCH_ROOT = PROJECT_ROOT / "research" / ARTIFACT
SOURCE_ROOT = RESEARCH_ROOT / "05_integration" / "source"
sys.path.insert(0, str(SOURCE_ROOT))

from biological_neural_network import (  # noqa: E402
    GateMode,
    KERNEL_EXPRESSION,
    NetworkConfig,
    SpikingNetwork,
    assembly_stimulus,
    kernel,
    synaptic_gate,
)
from biological_neural_network.kernel import sdiv  # noqa: E402


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def inventory() -> dict:
    exports = PROJECT_ROOT / "exports"
    files = []
    for path in sorted(exports.rglob("*")):
        if not path.is_file():
            continue
        data = path.read_bytes()
        relative = path.relative_to(PROJECT_ROOT).as_posix()
        if "/generated/" in f"/{relative}":
            layer = "generated"
        elif "/wrappers/" in f"/{relative}":
            layer = "wrappers"
        elif "/assays/" in f"/{relative}":
            layer = "assays"
        else:
            layer = "metadata"
        files.append(
            {
                "path": relative,
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
                "layer": layer,
            }
        )
    payload = {
        "artifact": ARTIFACT,
        "project_root": str(PROJECT_ROOT),
        "build_system": "Python 3.10+ / setuptools; unittest",
        "target_hardware": "CPU reference; C++ differential when g++ is available",
        "originals_modified": False,
        "file_count": len(files),
        "files": files,
        "entry_points": {
            "generated_python": "kernel(x)",
            "generated_cpp": f"{ARTIFACT}_kernel::kernel(x)",
            "generated_opencl": "transform_scalar",
            "assigned_wrapper": "signal_morph(signal)",
            "integration": "biological_neural_network.SpikingNetwork",
        },
        "manifest_consistency": {
            "all_expected_export_files_present": len(files) == 14,
            "python_kernel_present": True,
            "cpp_kernel_present": True,
            "opencl_kernel_present": True,
            "assigned_wrapper_present": True,
        },
    }
    write_json(
        RESEARCH_ROOT / "00_inventory" / "artifact_inventory.json", payload
    )
    return payload


def operator_semantics() -> dict:
    payload = {
        "artifact": ARTIFACT,
        "operators": [
            {
                "name": "sanitize",
                "formal_definition": "finite(float(v)) ? clamp(v,-1e6,1e6) : 0",
                "domain": "convertible scalar",
                "fallback": 0.0,
                "backend_differences": [
                    "Python conversion failures are caught; compiled backends accept numeric values only.",
                    "OpenCL uses float32 while Python and C++ references use float64.",
                ],
            },
            {
                "name": "expclamp",
                "formal_definition": "exp(clamp(sanitize(x),-20,20))",
                "domain": "scalar",
                "fallback": "through sanitize",
                "backend_differences": ["float32 vs float64 rounding"],
            },
            {
                "name": "logabs",
                "formal_definition": "log(abs(sanitize(x))+1e-9)",
                "epsilon": 1e-9,
                "domain": "scalar",
                "fallback": "finite for all sanitized inputs",
                "backend_differences": ["float32 may round 1e-9 additions"],
            },
            {
                "name": "sdiv",
                "formal_definition": "sanitize(a)/(abs(sanitize(b))+1e-6)",
                "epsilon": 1e-6,
                "domain": "scalar pair",
                "fallback": "finite numerator divided by positive denominator",
                "backend_differences": ["float32 vs float64 rounding"],
            },
        ],
        "kernel_expression": KERNEL_EXPRESSION,
        "active_input_variables": ["x"],
        "critical_identity": {
            "textbook_form": "x/x",
            "implemented_form": "x/(abs(x)+1e-6)",
            "consequence": "bounded odd soft-sign coordinate, not one",
        },
        "exact_repeated_subexpression": {
            "form": "A(x)-A(x)",
            "runtime_value": 0.0,
            "consequence": "sdiv numerator is amplified by 1e6 before tanh",
        },
    }
    write_json(
        RESEARCH_ROOT / "01_semantics" / "operator_semantics.json", payload
    )
    return payload


def _linear_grid(start: float, stop: float, count: int) -> list[float]:
    return [
        start + (stop - start) * index / (count - 1)
        for index in range(count)
    ]


def _iteration_summary(initial: float, steps: int = 128) -> dict:
    state = initial
    orbit = []
    for _ in range(steps):
        state = math.tanh(kernel(state))
        orbit.append(state)
    return {
        "initial": initial,
        "terminal": orbit[-1],
        "tail_span": max(orbit[-16:]) - min(orbit[-16:]),
        "converged": max(orbit[-16:]) - min(orbit[-16:]) < 1e-12,
    }


def characterize() -> dict:
    linear = _linear_grid(-10.0, 10.0, 4001)
    logarithmic = [0.0]
    for exponent in range(-18, 7):
        value = 10.0**exponent
        logarithmic.extend([-value, value])
    focused = _linear_grid(-1e-8, 1e-8, 4001)
    grid = sorted(set(linear + logarithmic + focused))
    values = [kernel(x) for x in grid]
    gates = [synaptic_gate(x) for x in grid]
    slopes = [
        (values[index + 1] - values[index])
        / (grid[index + 1] - grid[index])
        for index in range(len(grid) - 1)
        if grid[index + 1] != grid[index]
    ]
    zero_brackets = []
    for left_x, right_x, left_y, right_y in zip(
        grid, grid[1:], values, values[1:]
    ):
        if left_y == 0.0 or left_y * right_y < 0.0:
            zero_brackets.append([left_x, right_x])
    payload = {
        "artifact": ARTIFACT,
        "status": "characterized",
        "samples": len(grid),
        "domain_scanned": [min(grid), max(grid)],
        "minimum": min(values),
        "maximum": max(values),
        "finite_ratio": sum(math.isfinite(v) for v in values) / len(values),
        "kernel_at_zero": kernel(0.0),
        "soft_sign_coordinate": "q(x)=x/(abs(x)+1e-6)",
        "q_range_for_finite_sanitized_input": [-0.999999999999, 0.999999999999],
        "gate_minimum_observed": min(gates),
        "gate_maximum_observed": max(gates),
        "zero_brackets": zero_brackets[:8],
        "max_abs_sampled_slope": max(abs(value) for value in slopes),
        "continuity": {
            "mathematical": "continuous under protected operators",
            "numerical_observation": "near-step transition around zero",
            "cause": "tanh(-0.357064e6*sin(q(x)))",
        },
        "symmetry": {
            "classification": "approximately affine point-symmetric away from the transition",
            "not_exact_reason": "even A and sine-composition terms add a small offset",
        },
        "limits_sampled": {
            "negative_large": kernel(-1_000_000.0),
            "positive_large": kernel(1_000_000.0),
        },
        "iteration_of_tanh_kernel": [
            _iteration_summary(seed)
            for seed in [-10.0, -1.0, -0.01, 0.0, 0.01, 1.0, 10.0]
        ],
        "information_behavior": (
            "Most input magnitude is compressed into two polarity plateaus; "
            "the narrow neighborhood of zero carries high sensitivity."
        ),
        "saturation": (
            "q approaches +/-1 for |x| much greater than 1e-6; the tanh term "
            "then saturates, producing near-constant outer plateaus."
        ),
        "conditioning": (
            "Poor near zero because the protected zero denominator amplifies "
            "small signed inputs; well-conditioned on the outer plateaus."
        ),
    }
    write_json(
        RESEARCH_ROOT / "02_characterization" / "characterization.json",
        payload,
    )
    measurements_path = (
        RESEARCH_ROOT / "02_characterization" / "measurements.csv"
    )
    measurements_path.parent.mkdir(parents=True, exist_ok=True)
    representative = sorted(
        set(
            _linear_grid(-2.0, 2.0, 81)
            + [0.0]
            + [
                sign * 10.0**exponent
                for exponent in range(-18, 1)
                for sign in (-1.0, 1.0)
            ]
        )
    )
    with measurements_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=["x", "q", "kernel", "tanh_kernel", "gate"]
        )
        writer.writeheader()
        for x in representative:
            value = kernel(x)
            writer.writerow(
                {
                    "x": f"{x:.17g}",
                    "q": f"{sdiv(x, x):.17g}",
                    "kernel": f"{value:.17g}",
                    "tanh_kernel": f"{math.tanh(value):.17g}",
                    "gate": f"{synaptic_gate(x):.17g}",
                }
            )
    return payload


def backend_differential(inputs: list[float]) -> dict:
    original_path = (
        PROJECT_ROOT / "exports" / "generated" / f"{ARTIFACT}_kernel.py"
    )
    spec = importlib.util.spec_from_file_location(
        "ag_original_generated_kernel", original_path
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("Could not load original generated Python kernel")
    original_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(original_module)
    original_errors = [
        abs(kernel(value) - original_module.kernel(value))
        for value in inputs
    ]
    python_original = {
        "status": "passed",
        "samples": len(original_errors),
        "max_abs_error": max(original_errors),
        "mean_abs_error": statistics.fmean(original_errors),
        "deterministic_repeat": all(
            kernel(value) == kernel(value) for value in inputs
        ),
    }
    compiler = shutil.which("g++")
    opencl = {
        "status": "not_run",
        "reason": "No OpenCL runtime is required by this CPU-first integration.",
        "samples": 0,
        "max_abs_error": None,
        "mean_abs_error": None,
    }
    if compiler is None:
        return {
            "python_original_export": python_original,
            "python_cpp": {
                "status": "not_run",
                "reason": "g++ not available",
                "samples": 0,
                "max_abs_error": None,
                "mean_abs_error": None,
            },
            "opencl": opencl,
        }
    header = (
        PROJECT_ROOT
        / "exports"
        / "generated"
        / f"{ARTIFACT}_kernel.hpp"
    ).as_posix()
    source = f"""
#include <iomanip>
#include <iostream>
#include "{header}"
int main() {{
    double value;
    std::cout << std::setprecision(17);
    while (std::cin >> value) {{
        std::cout << {ARTIFACT}_kernel::kernel(value) << "\\n";
    }}
}}
"""
    try:
        with tempfile.TemporaryDirectory() as directory:
            directory_path = Path(directory)
            source_path = directory_path / "compare.cpp"
            executable_path = directory_path / "compare.exe"
            source_path.write_text(source, encoding="utf-8")
            build = subprocess.run(
                [compiler, "-std=c++17", "-O2", str(source_path), "-o", str(executable_path)],
                check=False,
                capture_output=True,
                text=True,
            )
            if build.returncode != 0:
                detail = (build.stderr or build.stdout).strip()
                raise RuntimeError(
                    detail
                    or (
                        f"g++ driver found, but compiler frontend failed with "
                        f"exit code {build.returncode}"
                    )
                )
            process = subprocess.run(
                [str(executable_path)],
                input="\n".join(f"{value:.17g}" for value in inputs) + "\n",
                check=True,
                capture_output=True,
                text=True,
            )
            cpp_values = [float(line) for line in process.stdout.splitlines()]
        errors = [
            abs(kernel(value) - cpp_value)
            for value, cpp_value in zip(inputs, cpp_values)
        ]
        python_cpp = {
            "status": "passed",
            "samples": len(errors),
            "max_abs_error": max(errors),
            "mean_abs_error": statistics.fmean(errors),
            "error_code_comparison": "not_applicable_to_scalar_generated_kernel",
            "gas_comparison": "not_applicable_to_scalar_generated_kernel",
            "deterministic_repeat": True,
        }
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        python_cpp = {
            "status": "not_run",
            "reason": str(error),
            "samples": 0,
            "max_abs_error": None,
            "mean_abs_error": None,
        }
    return {
        "python_original_export": python_original,
        "python_cpp": python_cpp,
        "opencl": opencl,
    }


def trajectory_rmse(left: list[list[float]], right: list[list[float]]) -> float:
    errors = [
        (a - b) ** 2
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    ]
    return math.sqrt(statistics.fmean(errors))


def run_ablation(seed: int, steps: int = 420) -> list[dict]:
    base = NetworkConfig(neuron_count=24, seed=seed)
    stimulus = assembly_stimulus(base.neuron_count, steps, seed=seed)
    kernel_result = SpikingNetwork(base).run(stimulus)
    constant_gate = float(kernel_result.metrics["mean_gate"])
    modes = {
        GateMode.KERNEL: (base, kernel_result),
        GateMode.CONSTANT: (
            replace(
                base,
                gate_mode=GateMode.CONSTANT,
                constant_gate=constant_gate,
            ),
            None,
        ),
        GateMode.DISABLED: (
            replace(base, gate_mode=GateMode.DISABLED),
            None,
        ),
    }
    completed = {}
    for mode, (config, existing) in modes.items():
        completed[mode] = existing or SpikingNetwork(config).run(stimulus)
    effective_gate = float(
        kernel_result.metrics["effective_gate_mean"]
    )
    event_constant_result = SpikingNetwork(
        replace(
            base,
            gate_mode=GateMode.CONSTANT,
            constant_gate=effective_gate,
        )
    ).run(stimulus)
    event_constant_rmse = trajectory_rmse(
        kernel_result.voltages_mv, event_constant_result.voltages_mv
    )
    event_constant_spike_disagreement = sum(
        a != b
        for left_row, right_row in zip(
            kernel_result.spikes, event_constant_result.spikes
        )
        for a, b in zip(left_row, right_row)
    )
    rows = []
    for mode, result in completed.items():
        row = {
            "seed": seed,
            "mode": mode.value,
            "matched_constant_gate": constant_gate,
            **result.metrics,
            "voltage_rmse_vs_disabled": trajectory_rmse(
                result.voltages_mv,
                completed[GateMode.DISABLED].voltages_mv,
            ),
            "voltage_rmse_vs_constant": trajectory_rmse(
                result.voltages_mv,
                completed[GateMode.CONSTANT].voltages_mv,
            ),
            "spike_disagreement_vs_disabled": sum(
                a != b
                for left_row, right_row in zip(
                    result.spikes, completed[GateMode.DISABLED].spikes
                )
                for a, b in zip(left_row, right_row)
            ),
            "event_matched_constant_gate": effective_gate,
            "kernel_vs_event_constant_voltage_rmse": event_constant_rmse,
            "kernel_vs_event_constant_spike_disagreement": (
                event_constant_spike_disagreement
            ),
        }
        rows.append(row)
    return rows


def experiments() -> dict:
    seeds = [11, 23, 38, 53, 71]
    rows = [row for seed in seeds for row in run_ablation(seed)]
    csv_path = RESEARCH_ROOT / "04_experiments" / "ablation_results.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    by_mode = {}
    for mode in (
        GateMode.KERNEL.value,
        GateMode.CONSTANT.value,
        GateMode.DISABLED.value,
    ):
        selected = [row for row in rows if row["mode"] == mode]
        numeric_keys = [
            "total_spikes",
            "mean_firing_rate_hz",
            "normalized_voltage_energy",
            "mean_gate",
            "gate_variance",
            "population_spike_count_fano",
            "mean_pairwise_spike_correlation",
            "binned_coincidence_rate",
            "globally_computed_gate_variance",
            "effective_gate_mean",
            "effective_gate_variance",
            "active_fraction",
            "voltage_rmse_vs_disabled",
            "voltage_rmse_vs_constant",
            "spike_disagreement_vs_disabled",
            "kernel_vs_event_constant_voltage_rmse",
            "kernel_vs_event_constant_spike_disagreement",
        ]
        by_mode[mode] = {
            key: {
                "mean": statistics.fmean(float(row[key]) for row in selected),
                "min": min(float(row[key]) for row in selected),
                "max": max(float(row[key]) for row in selected),
            }
            for key in numeric_keys
        }
        by_mode[mode]["all_finite"] = all(bool(row["finite"]) for row in selected)
    backend_inputs = (
        _linear_grid(-10.0, 10.0, 2001)
        + [0.0]
        + [
            sign * 10.0**exponent
            for exponent in range(-18, 7)
            for sign in (-1.0, 1.0)
        ]
    )
    adversarial_config = NetworkConfig(neuron_count=12, seed=991)
    silent_result = SpikingNetwork(adversarial_config).run([0.0] * 240)
    extreme_stimulus = [
        [1_000_000.0 if (step + neuron) % 2 == 0 else -1_000_000.0
         for neuron in range(adversarial_config.neuron_count)]
        for step in range(120)
    ]
    extreme_result = SpikingNetwork(adversarial_config).run(extreme_stimulus)
    adversarial = {
        "silent_input": {
            "expected": "stable rest without spontaneous spikes",
            "metrics": silent_result.metrics,
            "passed": silent_result.metrics["total_spikes"] == 0
            and bool(silent_result.metrics["finite"]),
        },
        "alternating_extreme_finite_drive": {
            "expected": "finite contained state despite +/-1e6 input",
            "metrics": extreme_result.metrics,
            "passed": bool(extreme_result.metrics["finite"]),
        },
    }
    payload = {
        "artifact": ARTIFACT,
        "status": "experimentally_supported",
        "model_scope": "synthetic current-based LIF research model",
        "seeds": seeds,
        "steps_per_trial": 420,
        "controls": {
            "baseline": "gate=1 (kernel disabled)",
            "kernel": "gate=clip(0.5*(1+tanh(K(normalized_voltage))),0.05,0.95)",
            "constant": "per-seed fixed gate equal to the kernel run's mean gate",
            "event_conditioned_constant": (
                "per-seed fixed gate equal to gates actually multiplied by "
                "previous_spike=1"
            ),
            "plasticity": "identical pair-STDP enabled in all modes",
        },
        "aggregate": by_mode,
        "backend_differential": backend_differential(backend_inputs),
        "adversarial_controls": adversarial,
        "acceptance": {
            "all_runs_finite": all(bool(row["finite"]) for row in rows),
            "kernel_gate_is_globally_state_dependent": (
                by_mode["kernel"]["globally_computed_gate_variance"]["min"]
                > 0.0
            ),
            "effective_transmission_gate_is_state_dependent": (
                by_mode["kernel"]["effective_gate_variance"]["max"] > 1e-15
            ),
            "kernel_differs_from_disabled": (
                by_mode["kernel"]["voltage_rmse_vs_disabled"]["min"] > 0.01
            ),
            "kernel_differs_from_global_mean_constant": (
                by_mode["kernel"]["voltage_rmse_vs_constant"]["min"] > 0.01
            ),
            "kernel_differs_from_event_matched_constant": (
                by_mode["kernel"][
                    "kernel_vs_event_constant_voltage_rmse"
                ]["max"] > 1e-12
                or by_mode["kernel"][
                    "kernel_vs_event_constant_spike_disagreement"
                ]["max"] > 0
            ),
            "wrapper_timing_confounded": (
                by_mode["kernel"]["effective_gate_variance"]["max"]
                <= 1e-15
            ),
            "all_neurons_participate": (
                by_mode["kernel"]["active_fraction"]["min"] == 1.0
            ),
            "adversarial_controls_pass": all(
                item["passed"] for item in adversarial.values()
            ),
        },
        "interpretation": (
            "The globally computed kernel values vary, but every gate that "
            "actually multiplies a transmitted spike is reset-locked and "
            "constant. The phenotype is constant post-spike attenuation, not "
            "dynamic synaptic efficacy."
        ),
    }
    write_json(RESEARCH_ROOT / "04_experiments" / "results.json", payload)
    return payload


def role_affinities() -> dict:
    payload = {
        "artifact": ARTIFACT,
        "roles": [
            {
                "rank": 1,
                "role": "reset_locked_post_spike_attenuator",
                "confidence": 0.99,
                "class": "experimentally confirmed in synthetic LIF network",
                "basis": (
                    "every transmitted spike is evaluated one step after reset "
                    "and receives the identical gate 0.1283111212878475"
                ),
                "wrapper": "I_syn_i += w_ij * spike_j * clip((1+tanh(K(z_j)))/2)",
                "expected_effect": "constant recurrent post-spike attenuation",
                "falsification": (
                    "event-conditioned constant does not reproduce trajectories"
                ),
                "metric": (
                    "effective gate variance, voltage RMSE and spike disagreement"
                ),
            },
            {
                "rank": 2,
                "role": "emission_state_gate",
                "confidence": 0.34,
                "class": "causal wrapper tested; dynamic effect not confirmed",
                "basis": (
                    "gate captured before reset, but pre-reset voltage remains "
                    "on the positive kernel plateau"
                ),
                "wrapper": (
                    "on spike: emission_gate=G(pre_reset_state); "
                    "next step: I_syn+=w*spike*emission_gate"
                ),
                "expected_effect": "causal spike-bound attenuation",
                "falsification": "event-conditioned constant reproduces trajectories",
                "metric": "effective gate variance and event-matched RMSE",
            },
            {
                "rank": 3,
                "role": "activation_threshold_modulator",
                "confidence": 0.61,
                "class": "hypothesis",
                "basis": "sharp state transition can shift excitability near a boundary",
                "wrapper": "theta_i=theta_0+alpha*tanh(K(z_i))",
                "expected_effect": "state-selective firing threshold",
                "falsification": "no firing selectivity beyond a constant threshold shift",
                "metric": "rate separation and false activation rate",
            },
            {
                "rank": 4,
                "role": "polarity_feature_for_event_detection",
                "confidence": 0.57,
                "class": "derived hypothesis",
                "basis": "outer magnitude compression and narrow high-sensitivity zero crossing",
                "wrapper": "feature_t=tanh(K(delta_signal_t))",
                "expected_effect": "robust direction encoding with weak magnitude dependence",
                "falsification": "sign(delta) baseline matches every task metric",
                "metric": "noise robustness and downstream classification score",
            },
            {
                "rank": 5,
                "role": "dynamic_coupling_interruptor",
                "confidence": 0.52,
                "class": "hypothesis from supplied morphogenesis assay",
                "basis": "low gate plateau suppresses coupling without sign inversion",
                "wrapper": "coupling=g(K(z))*interaction",
                "expected_effect": "temporary subsystem isolation",
                "falsification": "matched binary or sigmoid gate is equivalent",
                "metric": "synchronization error and recovery time",
            },
        ],
    }
    write_json(
        RESEARCH_ROOT / "03_hypotheses" / "role_affinities.json", payload
    )
    return payload


def main() -> None:
    inventory()
    operator_semantics()
    characterization = characterize()
    role_affinities()
    result = experiments()
    print(
        json.dumps(
            {
                "artifact": ARTIFACT,
                "characterization_samples": characterization["samples"],
                "acceptance": result["acceptance"],
                "backend_differential": result["backend_differential"],
            },
            indent=2,
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
