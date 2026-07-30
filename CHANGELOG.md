# Changelog

All notable changes to TATARUS are documented in this file.

## Runenkrieg_Tatarus 1.4.0 – 2026-07-30

First scientific Android subproject release of **Runenkrieg × TATARUS**.

### Included

- Fully offline Android card game and TATARUS laboratory.
- Mobile persistent spiking core with 72 neurons and 432 directed synapses.
- Complete neuronal wiring of all 32 game and candidate input channels.
- Pure TATARUS as the default opponent without rule-score mixing.
- Hybrid, rule, random, frozen and mechanism-ablation modes.
- Event-causal generated-operator transmission and constant-gate control.
- Local signed eligibility memory, short-term synaptic state and reward
  modulation.
- Competitive assemblies with entropy, separation and reactivation metrics.
- Full-game, learning-free evaluation on identical deterministic seeds.
- Separate real-game, self-training, neural, energy and eligibility metrics.
- German technical documentation and scientific whitepaper.

### Validation

- JVM unit tests passed.
- Android Lint passed.
- Debug and R8 release builds passed from the new repository subdirectory.
- The instrumented full-game test APK was built. No device was connected
  during the final clean-repository run, so that run did not execute the
  device phase.

### Research status

The integration verifies a technically functional, persistent and
ablatable Android agent. It does not by itself establish statistically
significant strategic superiority, biological equivalence or general
intelligence.

## 1.0.0 – 2026-07-29

First public research release of **TATARUS – A Persistent Synthetic Nervous
System**.

### Included

- Persistent C++ synthetic nervous-system core and native Windows UI.
- Deterministic spiking E/I network with configurable neurodynamics.
- Event-causal generated-operator synapses and required control gates.
- STDP, local synaptic eligibility memory, structural plasticity and repair.
- Raw multimodal sensor frames, assemblies, actions and composite snapshots.
- Temporal readouts, Delayed-XOR and trace-essential memory experiments.
- Research stages 1–23 with reports, controls and replication material.
- Python integration package and test suites.

### Research status

TATARUS is an experimental synthetic research system. Published results are
limited to the documented synthetic tasks and do not establish biological
equivalence or general intelligence.
