# Integrationsplan und Rückfallpfad

## Sicherer Integrationspunkt

Der Originalkernel wird ausschließlich in der Berechnung präsynaptischer
Wirksamkeitsfaktoren aufgerufen. Zwei bewusst getrennte Timingvarianten stehen
zur Verfügung:

```text
RESET_LOCKED:
  previous_spike -> aktueller Reset-Zustand -> Gate -> Übertragung

EMISSION_STATE:
  Schwellenüberschreitung -> Eventfeature und Gate vor Reset speichern
  -> SpikeEvent im Folgeschritt mit previous_spike übertragen
```

Die nichtnegative Projektion verhindert Vorzeichenwechsel und erhält damit das
Dale-Prinzip. Gewichte werden auch unter STDP auf ihr ursprüngliches
präsynaptisches Vorzeichen projiziert.

## Stabile API

- `NetworkConfig`: validierte, unveränderliche Konfiguration
- `SpikingNetwork.step(...)`: ein Millisekunden-Schritt
- `SpikingNetwork.run(...)`: vollständige Zeitreihe
- `SimulationResult`: Spannungen, Spikes, Gates, Endgewichte und Metriken
- `SpikeEvent`: Quelle, Emissionsschritt, Amplitude, Gate und Featurewert
- `GateMode.KERNEL`: experimentelle Integration
- `GateMode.CONSTANT`: Mittelwertkontrolle
- `GateMode.DISABLED`: Rückfallpfad ohne Kernel
- `GateTiming.RESET_LOCKED`: bewusster resetgebundener Dämpfer
- `GateTiming.EMISSION_STATE`: kausal am Spike gespeichertes Gate
- `GatePerturbation.TIME_SHIFTED`: zeitliche Kontrolle
- `GatePerturbation.STATE_SHUFFLED`: neuronale Zuordnungskontrolle
- `EmissionFeature.PRE_RESET_VOLTAGE`: frühere Emissionsvariante
- `EmissionFeature.EI_BALANCE`: dynamische normalisierte E/I-Balance
- `EmissionFeature.FEATURE_PROJECTION`: gewichtete kausale Projektion aus
  E/I-Balance, Membransteigung, Schwellenüberschuss und ISI-Zustand
- `SynapseModel.CURRENT_BASED`: eingefrorene strombasierte Referenz
- `SynapseModel.CONDUCTANCE_BASED`: AMPA-/GABA-Umkehrpotentialmodell
- verbindungsspezifische Axonverzögerungen aus einem konfigurierbaren Bereich
- optionales passives Soma-Dendrit-Zweikompartiment
- getrennte `GateMode`-Zuweisungen für `EE`, `EI`, `IE` und `II`

## Deaktivierung

Der Kernel kann ohne Codeänderung mit `gate_mode=GateMode.DISABLED` bzw.
`--gate disabled` deaktiviert werden. Dabei wird `g=1` eingesetzt; keine
bestehende Netzwerkfunktion wird ersetzt.

Die Standard-Konstantkontrolle für `RESET_LOCKED` ist
`0,12831112128784755`, nicht das globale Gate-Mittel. Für
`EMISSION_STATE` liegt das gemessene event-konditionierte Mittel bei ungefähr
`0,88934049`. Bei der neuen E/I-Variante muss der Event-Mittelwert je
Kalibrationslauf bestimmt werden; die C++-Vergleichsfunktion erledigt dies
automatisch und übernimmt für das Zufallsgate zusätzlich die empirische
wirksame Gateverteilung.

## Backend

Die CPU-Referenz ist das Orakel. Der unveränderte Python-Export stimmt im
Differentialtest exakt mit der Integration überein. Der zuerst erkannte
MinGW-Compiler hatte ein nicht ausführbares Frontend und bleibt für den alten
Differentiallauf korrekt als `not_run` dokumentiert. Anschließend wurde die
native C++-Engine mit MSVC 2022 erfolgreich gebaut. Ihre Tests prüfen bekannte
Kernelwerte, Determinismus, Endlichkeit, Dale-Prinzip und den vollständigen
Cross-Validation-Pfad. OpenCL wurde weiterhin nicht ausgeführt. Deshalb bleibt
der Status experimentell, nicht produktionsreif.

## Grenzen

- dimensionslose, current-based LIF-Näherung statt biophysikalischem
  Kompartimentmodell,
- keine realen neuronalen Messdaten,
- keine Leitungsverzögerungen oder dendritischen Kompartimente,
- bisher nur synthetische Reihenfolgeerkennung mit linearem Readout,
- CPU-only validiert.
