# Forschungsstufe 17 – autonome Repräsentation und Wiederherstellung

Status: `measured_experimental`

## Repräsentationsbildung

| Metrik | Wert |
|---|---:|
| Ähnlichkeit gleicher Reize über Zeit | 0.923732 |
| Ähnlichkeit verschiedener Reize | -0.527713 |
| Reaktivierung durch ähnlichen Reiz | 0.860225 |
| Audio-only zu gelernter multimodaler Repräsentation | 0.568144 |
| Repräsentationsähnlichkeit nach Schaden | 0.732716 |
| Assemblies vorher / nachher | 0 / 0 |
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
- striktes Trace-essential-Kriterium erfüllt: nein

Das Readout erhielt nur Spikeänderungen und Membranzustände im neutralen Recallfenster nach 400 reizfreien Schritten. Cue-Features, Eligibility-Werte und Interaktionsprodukte wurden nicht ausgegeben.

## Funktionsverlust und Wiederherstellung

- Funktion vor Schaden: 0.035642
- unmittelbar nach Schaden: 0.022259
- nach Erholungslernen: -0.017524
- gemessener Funktionsverlust: 0.013382
- wiedergewonnener Anteil: -2.972830
- Jaccard der 100 meistgenutzten Pfade: 0.162791
- deaktivierte Neuronen / Synapsen: 7 / 176
- neue aktive / tatsächlich genutzte Synapsen: 170 / 151
- alternative Lösung nach Kriterium: nein

Alle Werte sind experimentelle Messungen dieses synthetischen Systems. Ein positiver Einzelwert ist ohne Mehrseed-Bestätigung keine Überlegenheits- oder Biologiebehauptung.
