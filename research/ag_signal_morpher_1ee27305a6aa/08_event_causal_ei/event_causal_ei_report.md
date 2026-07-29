# Forschungsstufe 2: ereigniskausale E/I-Synapse

## Implementierter Mechanismus

Ein Spike ist nun ein explizites Ereignis:

```cpp
struct SpikeEvent {
    std::uint32_t source_neuron;
    std::uint32_t emission_step;
    double amplitude;
    double generated_gate;
    double feature_value;
};
```

Bei der Schwellenüberschreitung und vor dem Reset wird die rekurrente
E/I-Balance des emittierenden Neurons erfasst:

```text
b = (I_exc - I_inh) / (|I_exc| + |I_inh| + 1e-9)
```

Der daraus berechnete Gatewert wird zusammen mit dem Spike gespeichert und im
folgenden Übertragungsschritt verwendet. Exzitatorische und inhibitorische
synaptische Zustände werden dafür getrennt geführt. `RESET_LOCKED` bleibt als
unveränderte Referenzarchitektur verfügbar.

## Versuchsaufbau

- fünf unabhängige Netzwerk-Seeds: `11, 23, 38, 53, 71`
- Dynamikassay: 16 Neuronen, 420 Schritte, Assembly-Stimulus, STDP aus
- Reihenfolgeerkennung: 12 Samples je Klasse, drei Folds, 120 Schritte
- Pflichtkontrollen: event-gematchte Konstante, Vorzeichen, `tanh`,
  verteilungsgematchtes Zufallsgate, zeitverschoben, state-shuffled und
  deaktiviert

Die vollständigen Einzelergebnisse stehen in
`dynamics_ablation.csv` und `classification_ablation.csv`.

## Zentrale Ergebnisse

| Variante | Accuracy | Gate-Varianz | Gate-Entropie | Assembly-Separation | Spikes/korrekte Entscheidung |
|---|---:|---:|---:|---:|---:|
| Originalkernel mit E/I | 90,83 % | 0,06980 | 0,8546 bit | 2,6417 | 11,6909 |
| Event-Konstante | **95,00 %** | 0 | 0 bit | 2,5865 | **11,3795** |
| Verteilungsgematchtes Zufallsgate | 91,67 % | 0,05844 | 0,8634 bit | 2,6489 | 11,6290 |
| `tanh` | 90,83 % | 0,09395 | 1,0990 bit | **2,7113** | 11,8650 |
| deaktiviert | 88,33 % | 0 | 0 bit | 2,4274 | 13,9091 |
| zeitverschoben | 88,33 % | praktisch 0 | 0 bit | 2,3906 | 12,9435 |
| Vorzeichen | 87,50 % | 0,08270 | 0,6018 bit | 2,4056 | 13,1181 |
| state-shuffled | 85,83 % | 0,09557 | 0,9441 bit | 2,4178 | 12,2821 |

## Befund

Die erste Voraussetzung der nächsten Forschungsstufe ist erfüllt:

```text
effective_gate_variance = 0.0697995
effective_gate_entropy  = 0.854631 bit
```

Der Originaloperator wird am wirksamen Spikeereignis tatsächlich dynamisch.
Die E/I-Balance wechselt im Assay das Vorzeichen und nutzt beide
Kernelplateaus.

Ein einzigartiger Aufgabenvorteil ist dagegen nicht belegt. Die
event-gematchte Konstante erreicht in der Reihenfolgeerkennung `95,00 %`,
der dynamische Originalkernel `90,83 %`. Auch die Energiekennzahl ist für die
Konstante leicht besser. Die besondere Kernelgeometrie wird deshalb in dieser
Stufe nicht als überlegen eingestuft.

## Entscheidung

`KEEP_AND_RESEARCH`

Die ereigniskausale Infrastruktur ist ein belastbarer Fortschritt und bleibt
die neue Experimentierbasis. Der nächste zulässige Schritt ist eine
Feature-Projektion aus E/I-Balance, Membransteigung, Schwellenüberschuss und
Inter-Spike-Intervall. Jede Projektion muss erneut gegen dieselben
event-konditionierten Kontrollen geprüft werden.
