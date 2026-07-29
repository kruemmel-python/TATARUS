# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung

Status: `measured_experimental`

## Repräsentationsbildung

| Metrik | Wert |
|---|---:|
| Ähnlichkeit gleicher Reize über Zeit | 0.786267 |
| Ähnlichkeit verschiedener Reize | 0.986085 |
| Reaktivierung durch ähnlichen Reiz | 0.926730 |
| Audio-only zu gelernter multimodaler Repräsentation | 0.920411 |
| Repräsentationsähnlichkeit nach Schaden | 0.846052 |
| Assemblies vorher / nachher | 3 / 3 |
| Snapshot bitgenau | ja |

## Tokenizerfreie Sequenzbildung

- Übergangsklassifikation auf unberührten Epochen: 0.104167
- Übergangsklassen: 6
- Grenzreaktion / normale Übergangsreaktion: 0.970305
- Stabilität AB früh zu spät: 0.938861

## Reizfreies Recall-Gedächtnis

- Accuracy mit lokaler Eligibility: 0.500000
- Accuracy ohne lokale Eligibility: 0.458333
- Vorteil: 0.041667
- striktes Trace-essential-Kriterium erfüllt: nein

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: 0.000000
- unmittelbar nach Schaden: 0.000000
- nach Erholungslernen: 0.000137
- gemessener Funktionsverlust: 0.000000
- wiedergewonnener Anteil: 0.000000
- Jaccard der 100 meistgenutzten Pfade: 0.036269
- deaktivierte Neuronen / Synapsen: 7 / 158
- neue aktive / tatsächlich genutzte Synapsen: 170 / 141
- alternative Lösung nach Kriterium: ja

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
