"""Biologically inspired spiking network with an experimental AG kernel gate."""

from .kernel import KERNEL_EXPRESSION, kernel, synaptic_gate
from .network import (
    EmissionFeature,
    GateMode,
    GatePerturbation,
    GateTiming,
    NetworkConfig,
    SimulationResult,
    SpikeEvent,
    SpikingNetwork,
)
from .stimuli import assembly_stimulus
from .classification import (
    ClassificationConfig,
    GateEvaluation,
    TemporalClassifier,
    TemporalDataset,
)

__all__ = [
    "GateMode",
    "EmissionFeature",
    "GatePerturbation",
    "GateTiming",
    "KERNEL_EXPRESSION",
    "ClassificationConfig",
    "GateEvaluation",
    "NetworkConfig",
    "SimulationResult",
    "SpikeEvent",
    "SpikingNetwork",
    "TemporalClassifier",
    "TemporalDataset",
    "assembly_stimulus",
    "kernel",
    "synaptic_gate",
]
