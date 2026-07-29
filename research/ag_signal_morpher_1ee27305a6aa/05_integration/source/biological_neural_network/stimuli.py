"""Deterministic stimulus protocols for the spiking network."""

from __future__ import annotations

import random


def assembly_stimulus(
    neuron_count: int,
    steps: int,
    *,
    seed: int = 38,
    baseline: float = 13.5,
    pulse: float = 10.0,
    noise_std: float = 0.6,
    period_steps: int = 60,
) -> list[list[float]]:
    """Create alternating assembly pulses with seeded background noise."""

    if neuron_count <= 0 or steps < 0:
        raise ValueError("neuron_count must be positive and steps non-negative")
    if period_steps < 4:
        raise ValueError("period_steps must be at least 4")
    rng = random.Random(seed)
    split = max(1, neuron_count // 2)
    stimulus: list[list[float]] = []
    active_width = period_steps // 2
    for step in range(steps):
        phase = (step // active_width) % 2
        row = []
        for neuron in range(neuron_count):
            belongs_to_active = (neuron < split) == (phase == 0)
            drive = baseline + (pulse if belongs_to_active else 0.0)
            row.append(drive + rng.gauss(0.0, noise_std))
        stimulus.append(row)
    return stimulus

