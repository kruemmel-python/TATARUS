# Runenkrieg × TATARUS Android Lab 1.4.0

This release adds **Runenkrieg_Tatarus** as a scientific Android subproject
of **TATARUS – A Persistent Synthetic Nervous System**.

## Why this subproject matters

Runenkrieg is both a playable card game and a closed experimental
environment. The same local TATARUS instance:

- observes structured game events,
- evaluates legal cards and fusions through neuronal rollouts,
- commits only the selected action to its persistent state,
- receives delayed consequences,
- updates readout and local synaptic state,
- survives Android app restarts,
- and can be compared against causal controls in the integrated laboratory.

The application therefore combines an environment, a persistent learning
agent, an instrumentation surface and an ablation testbed on one mobile
device.

## TATARUS mobile core

- 72 neurons
- 432 directed, Dale-constrained synapses
- all 32 input channels neuronally connected
- passive dendritic state
- individual axonal delays
- adaptive thresholds and homeostasis
- event-causal generated-operator gate
- signed local eligibility memory
- synaptic resources, depression and facilitation
- reward-modulated plasticity and slow consolidation state
- competitive assemblies
- modeled spike and transmission energy
- bounded action-specific readout
- complete deterministic model snapshots

## Research modes

- Pure TATARUS
- Hybrid 55/35/10
- Rule only
- Random
- Frozen TATARUS
- Without eligibility
- Without generated operator
- Without assemblies

The built-in evaluation compares seven applicable modes on identical
complete games with learning and exploration disabled. It reports wins,
draws, losses, token swing, rounds, spikes, transmissions and modeled
energy cost per game.

## Documentation

- `README.md` – build and usage
- `Tatarus_Runenkrieg_Dokumentation.md` – detailed technical documentation
- `Whitepaper_TATARUS_Runenkrieg_DE.md` – scientific motivation, methods,
  experimental design and limitations

## Validation

The following completed successfully from the clean
`Runenkrieg_Tatarus` repository subdirectory:

```powershell
.\gradlew.bat testDebugUnitTest
.\gradlew.bat lintDebug
.\gradlew.bat assembleDebug
.\gradlew.bat assembleRelease
```

The Android instrumented test APK also compiled. No Android device was
connected during the final repository build, so
`connectedDebugAndroidTest` could not execute in that final run.

## Release assets

- `Runenkrieg-TATARUS-v1.4.0-debug.apk` is installable and uses the standard
  Android debug signature. It is intended for research evaluation, not for
  production distribution or Play Store updates.
- `Runenkrieg-TATARUS-v1.4.0-release-unsigned.apk` is the optimized R8
  release artifact and must be signed by a trusted release key before
  installation.
- `SHA256SUMS.txt` contains the checksums of both APK files.

## Persistence migration

The Android model uses persistence schema version 3. An older incompatible
opponent state is reset when this version starts for the first time.

## Scientific scope

This release demonstrates a technically functional, persistent,
trainable and ablatable spiking opponent on Android. It does not establish:

- statistically significant superiority over simpler baselines,
- biological equivalence to a nervous system,
- general intelligence,
- consciousness or sentience,
- or transfer beyond the documented game environment.

Independent training seeds, untouched evaluation seeds, confidence
intervals, effect sizes and external replication are required before a
strategic superiority claim.

## License and author

Apache License 2.0
Developer: Ralf Krümmel
