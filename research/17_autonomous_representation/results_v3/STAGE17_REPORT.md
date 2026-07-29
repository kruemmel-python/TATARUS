# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung

Status: `measured_experimental`

## Repräsentationsbildung

| Metrik | Wert |
|---|---:|
| Ähnlichkeit gleicher Reize über Zeit | 0.540468 |
| Ähnlichkeit verschiedener Reize | 0.981817 |
| Reaktivierung durch ähnlichen Reiz | 0.012538 |
| Audio-only zu gelernter multimodaler Repräsentation | 0.006536 |
| Repräsentationsähnlichkeit nach Schaden | 0.013115 |
| Assemblies vorher / nachher | 1 / 1 |
| Snapshot bitgenau | ja |

## Tokenizerfreie Sequenzbildung

- Übergangsklassifikation auf unberührten Epochen: 0.104167
- Übergangsklassen: 6
- Grenzreaktion / normale Übergangsreaktion: 0.976848
- Stabilität AB früh zu spät: 0.769795

## Reizfreies Recall-Gedächtnis

- Accuracy mit lokaler Eligibility: 0.500000
- Accuracy ohne lokale Eligibility: 0.500000
- Vorteil: 0.000000
- striktes Trace-essential-Kriterium erfüllt: nein

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: 0.003514
- unmittelbar nach Schaden: 0.002418
- nach Erholungslernen: -0.058099
- gemessener Funktionsverlust: 0.001096
- wiedergewonnener Anteil: -55.238107
- Jaccard der 100 meistgenutzten Pfade: 0.282051
- deaktivierte Neuronen / Synapsen: 7 / 176
- neue aktive / tatsächlich genutzte Synapsen: 173 / 152
- alternative Lösung nach Kriterium: nein

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
