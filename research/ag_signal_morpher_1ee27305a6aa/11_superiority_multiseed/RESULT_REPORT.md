# Ergebnis des vorab festgelegten Mehrseed-Tests

Experiment-Hash: `3974457A05BA475D`

Statistische Einheit: Mittel der vier Bedingungen je Seed (`n = 24` neue Seeds).

## Modusmittel

| Modus | Accuracy | Spikes/korrekt | Rate Hz | Separation |
|---|---:|---:|---:|---:|
| kernel | 0.904297 | 8.334188 | 3.671208 | 3.390262 |
| constant | 0.904297 | 8.391204 | 3.695283 | 3.395756 |
| disabled | 0.905924 | 8.864264 | 3.913258 | 3.424312 |
| sign | 0.904622 | 8.316918 | 3.666687 | 3.389628 |
| tanh | 0.904948 | 8.439888 | 3.714780 | 3.395481 |
| random | 0.900391 | 8.408236 | 3.692910 | 3.399155 |

## Gepaarte Vergleiche Kernel minus Kontrolle

| Kontrolle | ΔAccuracy | untere 95-%-Grenze | p Holm Accuracy | Kostenvorteil | p Holm Kosten |
|---|---:|---:|---:|---:|---:|
| constant | 0.000000 | -0.002279 | 1.000000 | 0.057016 | 0.003033 |
| disabled | -0.001628 | -0.005534 | 1.000000 | 0.530076 | 0.000005 |
| sign | -0.000326 | -0.001953 | 1.000000 | -0.017270 | 0.927827 |
| tanh | -0.000651 | -0.002604 | 1.000000 | 0.105700 | 0.000005 |
| random | 0.003906 | 0.000326 | 0.264115 | 0.074048 | 0.005814 |

## Vorab definierte Entscheidungen

- Formelgeometrische Accuracy-Überlegenheit: **NICHT BESTÄTIGT**
- Systemische Effizienzüberlegenheit gegen Konstante und deaktiviert: **BESTÄTIGT**
- Formelspezifische Effizienzüberlegenheit gegen Zufall: **BESTÄTIGT**
- Effizienzüberlegenheit gegen alle fünf Kontrollen: **NICHT BESTÄTIGT**

## Zulässige Schlussfolgerung

Unter nichtunterlegener Accuracy besitzt der Originalkernel in dieser synthetischen Aufgabenfamilie einen Spikekostenvorteil gegenüber event-gematchter Konstante, deaktiviertem Gate und verteilungsgematchtem Zufallsgate. Das Vorzeichengate ist im Mittel sparsamer; deshalb besteht keine Überlegenheit gegenüber allen Kontrollen.

Alle Aussagen gelten nur für das synthetische Forschungsmodell und sind keine biologische Validierung.
