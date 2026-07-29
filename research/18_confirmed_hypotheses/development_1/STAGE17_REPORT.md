# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung

Status: `measured_experimental`

## Repräsentationsbildung

| Metrik | Wert |
|---|---:|
| Ähnlichkeit gleicher Reize über Zeit | 0.923710 |
| Ähnlichkeit verschiedener Reize | -0.527390 |
| Reaktivierung durch ähnlichen Reiz | 0.860234 |
| Audio-only zu gelernter multimodaler Repräsentation | 0.568204 |
| Repräsentationsähnlichkeit nach Schaden | 0.732791 |
| Assemblies vorher / nachher | 1 / 1 |
| Snapshot bitgenau | ja |

## Tokenizerfreie Sequenzbildung

- Übergangsklassifikation auf unberührten Epochen: 0.687500
- Übergangsklassen: 6
- Grenzreaktion / normale Übergangsreaktion: 2.893759
- Stabilität AB früh zu spät: 0.963064

## Reizfreies Recall-Gedächtnis

- Accuracy mit lokaler Eligibility: 0.500000
- Accuracy ohne lokale Eligibility: 0.500000
- Vorteil: 0.000000
- mittlere absolute Eligibility beim Recall: 0.001720
- striktes Trace-essential-Kriterium erfüllt: nein

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: 0.016286
- unmittelbar nach Schaden: 0.012277
- nach Erholungslernen: -0.019400
- gemessener Funktionsverlust: 0.004010
- wiedergewonnener Anteil: -7.900341
- Jaccard der 100 meistgenutzten Pfade: 0.257862
- deaktivierte Neuronen / Synapsen: 7 / 176
- neue aktive / tatsächlich genutzte Synapsen: 174 / 155
- alternative Lösung nach Kriterium: nein

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
