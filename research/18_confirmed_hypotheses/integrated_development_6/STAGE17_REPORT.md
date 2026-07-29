# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung

Status: `measured_experimental`

## Repräsentationsbildung

| Metrik | Wert |
|---|---:|
| Ähnlichkeit gleicher Reize über Zeit | 0.929126 |
| Ähnlichkeit verschiedener Reize | -0.555098 |
| Reaktivierung durch ähnlichen Reiz | 0.880633 |
| Audio-only zu gelernter multimodaler Repräsentation | 0.444890 |
| Repräsentationsähnlichkeit nach Schaden | 0.752190 |
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
- mittlere absolute Eligibility beim Recall: 0.043522
- striktes Trace-essential-Kriterium erfüllt: ja

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: 0.011220
- unmittelbar nach Schaden: -0.000000
- nach Erholungslernen: 0.000001
- gemessener Funktionsverlust: 0.011220
- wiedergewonnener Anteil: 0.000050
- Jaccard der 100 meistgenutzten Pfade: 0.041667
- deaktivierte Neuronen / Synapsen: 5 / 27
- neue aktive / tatsächlich genutzte Synapsen: 1281 / 1198
- alternative Lösung nach Kriterium: nein

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
