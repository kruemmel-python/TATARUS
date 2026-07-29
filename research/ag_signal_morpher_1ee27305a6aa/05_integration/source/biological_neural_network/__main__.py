"""Command-line demonstration for the synthetic spiking network."""

from __future__ import annotations

import argparse
import json

from .network import (
    EmissionFeature,
    GateMode,
    GatePerturbation,
    GateTiming,
    NetworkConfig,
    SpikingNetwork,
)
from .stimuli import assembly_stimulus


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--steps", type=int, default=300)
    parser.add_argument("--neurons", type=int, default=24)
    parser.add_argument("--seed", type=int, default=38)
    parser.add_argument(
        "--gate",
        choices=[mode.value for mode in GateMode],
        default=GateMode.KERNEL.value,
    )
    parser.add_argument(
        "--timing",
        choices=[timing.value for timing in GateTiming],
        default=GateTiming.RESET_LOCKED.value,
    )
    parser.add_argument(
        "--gate-control",
        choices=[control.value for control in GatePerturbation],
        default=GatePerturbation.NONE.value,
    )
    parser.add_argument(
        "--emission-feature",
        choices=[feature.value for feature in EmissionFeature],
        default=EmissionFeature.PRE_RESET_VOLTAGE.value,
    )
    parser.add_argument(
        "--constant-gate", type=float, default=0.12831112128784755
    )
    parser.add_argument("--projection-ei-weight", type=float, default=0.40)
    parser.add_argument("--projection-slope-weight", type=float, default=0.25)
    parser.add_argument(
        "--projection-overshoot-weight", type=float, default=0.15
    )
    parser.add_argument("--projection-isi-weight", type=float, default=0.20)
    parser.add_argument(
        "--membrane-slope-scale", type=float, default=1.0
    )
    parser.add_argument(
        "--threshold-overshoot-scale", type=float, default=1.0
    )
    parser.add_argument("--isi-tau-ms", type=float, default=50.0)
    parser.add_argument("--no-plasticity", action="store_true")
    args = parser.parse_args()
    config = NetworkConfig(
        neuron_count=args.neurons,
        seed=args.seed,
        gate_mode=GateMode(args.gate),
        gate_timing=GateTiming(args.timing),
        gate_perturbation=GatePerturbation(args.gate_control),
        emission_feature=EmissionFeature(args.emission_feature),
        projection_ei_weight=args.projection_ei_weight,
        projection_slope_weight=args.projection_slope_weight,
        projection_overshoot_weight=args.projection_overshoot_weight,
        projection_isi_weight=args.projection_isi_weight,
        membrane_slope_scale_mv_per_ms=args.membrane_slope_scale,
        threshold_overshoot_scale_mv=args.threshold_overshoot_scale,
        isi_tau_ms=args.isi_tau_ms,
        constant_gate=args.constant_gate,
        plasticity_enabled=not args.no_plasticity,
    )
    stimulus = assembly_stimulus(args.neurons, args.steps, seed=args.seed)
    network = SpikingNetwork(config)
    result = network.run(stimulus)
    payload = {
        "model": "synthetic_current_based_lif",
        "gate_mode": args.gate,
        "gate_timing": args.timing,
        "gate_control": args.gate_control,
        "emission_feature": args.emission_feature,
        "projection_weights": {
            "ei": args.projection_ei_weight,
            "slope": args.projection_slope_weight,
            "overshoot": args.projection_overshoot_weight,
            "isi": args.projection_isi_weight,
        },
        "dale_principle_holds": network.dale_principle_holds(),
        **result.metrics,
    }
    print(json.dumps(payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
