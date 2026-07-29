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
| Assemblies vorher / nachher | 6 / 7 |
| Snapshot bitgenau | ja |

## Tokenizerfreie Sequenzbildung

- Übergangsklassifikation auf unberührten Epochen: 0.812500
- Übergangsklassen: 6
- Grenzreaktion / normale Übergangsreaktion: 3.001718
- Stabilität AB früh zu spät: 0.982290

## Reizfreies Recall-Gedächtnis

- Entwicklungsaccuracy der gewählten Parameter: 1.000000
- gewähltes Tau / Gain / Inkrement: 800.000000 / 10.000000 / 20.000000
- Accuracy mit lokaler Eligibility: 1.000000
- Accuracy ohne lokale Eligibility: 0.486111
- Vorteil: 0.513889
- mittlere absolute Eligibility beim Recall: 0.043330
- striktes Trace-essential-Kriterium erfüllt: ja

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: -0.008930
- unmittelbar nach Schaden: -0.000000
- nach Erholungslernen: -0.003884
- gemessener Funktionsverlust: -0.008930
- wiedergewonnener Anteil: 0.434943
- Jaccard der 100 meistgenutzten Pfade: 0.047120
- deaktivierte Neuronen / Synapsen: 5 / 27
- neue aktive / tatsächlich genutzte Synapsen: 1279 / 1191
- alternative Lösung nach Kriterium: nein

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
