# Delayed-XOR-Replikation

Experiment-Hash: `C18E63A3EA120D7C`

24 neue Seeds, zwei Verzögerungsbedingungen und sechs Gatevarianten ergeben 288 vollständige Auswertungen.

## Modusmittel

| Modus | Accuracy | Spikes/korrekt | Rate Hz |
|---|---:|---:|---:|
| kernel | 0.512153 | 10.176464 | 1.310108 |
| constant | 0.513021 | 10.154038 | 1.313838 |
| disabled | 0.515625 | 10.787853 | 1.408330 |
| sign | 0.513455 | 10.119236 | 1.308978 |
| tanh | 0.511719 | 10.228720 | 1.326723 |
| random | 0.514757 | 10.155229 | 1.314177 |

## Kernel gegen Kontrollen

| Kontrolle | ΔAccuracy | untere 95-%-Grenze | p Holm Accuracy | Kostenvorteil | p Holm Kosten |
|---|---:|---:|---:|---:|---:|
| constant | -0.000868 | -0.008681 | 1.000000 | -0.022427 | 1.000000 |
| disabled | -0.003472 | -0.019965 | 1.000000 | 0.611388 | 0.040360 |
| sign | -0.001302 | -0.003906 | 1.000000 | -0.057228 | 1.000000 |
| tanh | 0.000434 | -0.007378 | 1.000000 | 0.052255 | 1.000000 |
| random | -0.002604 | -0.013021 | 1.000000 | -0.021235 | 1.000000 |

## Vorab festgelegte Entscheidungen

- Scoped-Effizienzreplikation: **NICHT BESTÄTIGT**
- starke Replikation gegen alle Kontrollen einschließlich Vorzeichen: **NICHT BESTÄTIGT**
- Accuracy-Überlegenheit gegen alle Kontrollen: **NICHT BESTÄTIGT**

## Schlussfolgerung

`NO_EFFICIENCY_REPLICATION`: Die vorab festgelegten Kriterien wurden auf Delayed XOR nicht vollständig erfüllt.

Die Aufgabe ist synthetisch; das Resultat ist keine biologische Validierung.
