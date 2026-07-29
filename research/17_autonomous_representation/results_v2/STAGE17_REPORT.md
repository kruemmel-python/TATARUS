# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung

Status: `measured_experimental`

## Repräsentationsbildung

| Metrik | Wert |
|---|---:|
| Ähnlichkeit gleicher Reize über Zeit | 0.039868 |
| Ähnlichkeit verschiedener Reize | -0.813175 |
| Reaktivierung durch ähnlichen Reiz | -0.153064 |
| Audio-only zu gelernter multimodaler Repräsentation | -0.153347 |
| Repräsentationsähnlichkeit nach Schaden | -0.134596 |
| Assemblies vorher / nachher | 1 / 1 |
| Snapshot bitgenau | ja |

## Tokenizerfreie Sequenzbildung

- Übergangsklassifikation auf unberührten Epochen: 0.229167
- Übergangsklassen: 6
- Grenzreaktion / normale Übergangsreaktion: 0.997855
- Stabilität AB früh zu spät: -0.880241

## Reizfreies Recall-Gedächtnis

- Accuracy mit lokaler Eligibility: 0.500000
- Accuracy ohne lokale Eligibility: 0.458333
- Vorteil: 0.041667
- striktes Trace-essential-Kriterium erfüllt: nein

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: 0.001040
- unmittelbar nach Schaden: 0.000708
- nach Erholungslernen: 0.002766
- gemessener Funktionsverlust: 0.000332
- wiedergewonnener Anteil: 6.196353
- Jaccard der 100 meistgenutzten Pfade: 0.092896
- deaktivierte Neuronen / Synapsen: 7 / 158
- neue aktive / tatsächlich genutzte Synapsen: 175 / 151
- alternative Lösung nach Kriterium: nein

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
