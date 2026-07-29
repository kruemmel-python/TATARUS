# Gate-Timing-Ablation

## Korrigierter Befund

Im ursprünglichen Update wird

```text
weight * previous_spike * gate(current_voltage)
```

berechnet. Ein `previous_spike=1` bedeutet, dass das betreffende Neuron im
vorigen Schritt sofort auf `-70 mV` zurückgesetzt wurde. Bei Ruhepotential
`-65 mV` und Schwelle `-50 mV` sieht der Kernel deshalb immer

```text
z_reset = (-70 + 65) / (-50 + 65) = -1/3
g_reset = 0.1283111212878475
```

## RESET_LOCKED

Über fünf Seeds ohne STDP:

- event-konditioniertes Gate-Mittel: `0,1283111212878475`,
- maximale event-konditionierte Varianz: `0`,
- maximale Spannungs-RMSE gegen die event-gematchte Konstante: `0`,
- gesamte Spikeabweichung: `0`.

**Experimentell bestätigt:** Der Phänotyp ist ein konstanter,
reset-/refraktärgekoppelter Post-Spike-Übertragungsdämpfer. Er ist im
vorhandenen Wrapper nicht dynamisch zustandsabhängig.

## EMISSION_STATE

Die neue kausale Variante speichert das Gate beim Spike vor dem Reset und
überträgt es zusammen mit `previous_spike` im nächsten Schritt.

Über fünf Seeds:

- event-konditioniertes Gate-Mittel: `0,8893404879`,
- maximale Varianz: `7,4×10^-17`,
- maximale Spannungs-RMSE gegen die event-gematchte Konstante:
  `2,8×10^-8`,
- Spikeabweichung: `0`.

Das Timing ist nun kausal korrekt, aber die normierte pre-reset Spannung ist
immer positiv und liegt erneut auf einem fast konstanten Kernelplateau.
`EMISSION_STATE` allein erzeugt mit diesem Feature daher noch keine praktisch
dynamische Modulation.

## Zusätzliche Kontrollen

- globale Konstante: unterscheidet sich deutlich, ist aber nicht
  event-gematcht,
- verteilungsgematchtes Zufallsgate: kollabiert bei Varianz null auf die
  event-gematchte Konstante,
- zeitverschobenes Gate: prüft die Auswertungsposition,
- state-shuffled Gate: erhält berechnete Werte, ändert aber ihre neuronale
  Zuordnung und führt zu deutlichen Trajektorienänderungen.

## Metrikkorrektur

Der bisherige „Synchronieindex“ heißt nun
`population_spike_count_fano`. Ergänzt wurden:

- `mean_pairwise_spike_correlation`,
- `binned_coincidence_rate`,
- globale Gateverteilung,
- event-konditionierte Gateverteilung.

## Wissenschaftliches Urteil

```text
kernel_gate_is_globally_state_dependent = true
effective_transmission_gate_is_state_dependent = false
kernel_differs_from_global_mean_constant = true
kernel_differs_from_event_matched_constant = false
wrapper_timing_confounded = true
```

Nächster sinnvoller Test ist ein am Spikezeitpunkt vorzeichenwechselndes
Feature, beispielsweise der signierte synaptische Nettostrom oder das
Exzitations-/Inhibitionsverhältnis.

Entscheidung: `KEEP_AND_RESEARCH`
