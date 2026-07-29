# Forschungsstufe 15: Trace-essential Memory

Experimenthash: `688F65FA0F77947C`

Eingefrorene Parameter: `tau=400 ms; Gain=0.5; Maximum=1`

Das Readout verwendet ausschließlich Spikecounts, Spiketraces und Membranspannungen aus `500...559 ms`. Cue-gebundene Eligibility und Produktmerkmale sind nicht enthalten.

| Kontrolle | Accuracy | Spikes/korrekt | ΔAccuracy Spur−Kontrolle | p Accuracy (Holm konservativ) | p Kosten (Holm konservativ) |
|---|---:|---:|---:|---:|---:|
| signed_trace | 0.635417 | 91.534899 | 0.000000 | 1.000000 | 1.000000 |
| without_trace | 0.500000 | 122.312500 | 0.135417 | 0.009766 | 0.004883 |
| gain_zero | 0.500000 | 122.312500 | 0.135417 | 0.009766 | 0.004883 |
| event_matched_constant | 0.500000 | 114.666667 | 0.135417 | 0.009766 | 0.004883 |
| absolute_trace | 0.523438 | 269.506728 | 0.111979 | 0.175781 | 0.009766 |
| time_shifted_trace | 0.807292 | 77.072563 | -0.171875 | 0.004883 | 0.009766 |
| synapse_shuffled_trace | 0.585938 | 108.192828 | 0.049479 | 1.000000 | 0.263672 |
| inverted_trace | 0.557292 | 162.365075 | 0.078125 | 0.195312 | 0.004883 |
| distribution_matched_random | 0.541667 | 109.103863 | 0.093750 | 0.322266 | 0.234375 |
| ee_only | 0.622396 | 93.380148 | 0.013021 | 1.000000 | 1.000000 |
| ie_only | 0.518229 | 120.623914 | 0.117188 | 0.014648 | 0.004883 |

## Vorab definierte Entscheidung

- Vollmodell-Accuracy ≥ 0,65: **NEIN**
- ohne Spur und Gain=0 ≤ 0,55: **JA**
- >0,03 und Holm-p<0,05 gegen jede Kontrolle: **NEIN**

Ergebnis: **TRACE_ESSENTIAL_MEMORY_NOT_CONFIRMED**

## Explorative Mechanik innerhalb der Spur-Familie

Die vorab definierte 40-ms-Timingkontrolle ist selbst eine lokale Eligibility-Spur, aber mit älterem synaptischem Zustand. Sie erreicht Accuracy `0.807292` gegenüber `0.500000` ohne Spur. Gepaarte Differenz: `0.307292`, unkorrektes exaktes p: `0.000488`.

Explorativer Befund lokaler Spur-Familien-Memory: **JA**. Dieser Befund ersetzt die negative vorab definierte Entscheidung nicht und benötigt neue Bestätigungsseeds.

Dies ist ein synthetischer Gedächtnistest und keine biologische Validierung.
