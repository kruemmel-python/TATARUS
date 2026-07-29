# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung

Status: `measured_experimental`

## Repräsentationsbildung

| Metrik | Wert |
|---|---:|
| Ähnlichkeit gleicher Reize über Zeit | 0.920267 |
| Ähnlichkeit verschiedener Reize | -0.529473 |
| Reaktivierung durch ähnlichen Reiz | 0.864624 |
| Audio-only zu gelernter multimodaler Repräsentation | 0.546628 |
| Repräsentationsähnlichkeit nach Schaden | 0.724936 |
| Assemblies vorher / nachher | 1 / 1 |
| Snapshot bitgenau | ja |

## Tokenizerfreie Sequenzbildung

- Übergangsklassifikation auf unberührten Epochen: 0.812500
- Übergangsklassen: 6
- Grenzreaktion / normale Übergangsreaktion: 3.001718
- Stabilität AB früh zu spät: 0.982290

## Reizfreies Recall-Gedächtnis

- Entwicklungsaccuracy der gewählten Parameter: 0.625000
- gewähltes Tau / Gain / Inkrement: 800.000000 / 6.000000 / 20.000000
- Accuracy mit lokaler Eligibility: 0.656250
- Accuracy ohne lokale Eligibility: 0.500000
- Vorteil: 0.156250
- mittlere absolute Eligibility beim Recall: 0.044087
- striktes Trace-essential-Kriterium erfüllt: nein

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: 0.028201
- unmittelbar nach Schaden: 0.016419
- nach Erholungslernen: -0.000334
- gemessener Funktionsverlust: 0.011782
- wiedergewonnener Anteil: -1.421904
- Jaccard der 100 meistgenutzten Pfade: 0.360544
- deaktivierte Neuronen / Synapsen: 7 / 176
- neue aktive / tatsächlich genutzte Synapsen: 174 / 153
- alternative Lösung nach Kriterium: nein

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
