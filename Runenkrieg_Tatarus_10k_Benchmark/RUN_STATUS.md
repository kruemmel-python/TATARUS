# Laufstatus des TATARUS-10k-Benchmarks

Stand: 31. Juli 2026

Der vorregistrierte, gerätenative Mehrseed-Lauf ist vollständig
abgeschlossen. Alle Ergebnisse wurden auf demselben registrierten Gerät
`RMX3853` mit ADB-Seriennummer `4c90bfcc` erzeugt.

## Bestätigte Checkpoints

| Checkpoint | bestätigte Seeds |
|---:|---:|
| 250 | 5/5 |
| 500 | 5/5 |
| 1.000 | 5/5 |
| 2.000 | 5/5 |
| 5.000 | 5/5 |
| 10.000 | 5/5 |

Damit sind 30 von 30 geplanten Lernkurvenpunkten vollständig geschrieben,
vom Gerät gezogen und lokal gesichert.

## Mehrseed-Lernkurve

| Runden | mittlere Partiensiegrate | 95-%-Bootstrapintervall |
|---:|---:|---:|
| 250 | 65 % | 54–74 % |
| 500 | 64 % | 56–71 % |
| 1.000 | 76 % | 69–82 % |
| 2.000 | 70 % | 66–74 % |
| 5.000 | 76 % | 66–84 % |
| 10.000 | 81 % | 75–86 % |

## Vorregistrierter Gewinner

Nach der vorab festgelegten Reihenfolge wurde Seed `20260732` ausgewählt:

- Auswahl-Holdout Seeds 30000–30019: 18/20 Siege = 90 %,
- mittlere Token-Differenz: +7,75,
- mittlere Entscheidungszeit: 148,982 ms,
- Snapshot-SHA-256:
  `98c5671ebfcf64734d4d791e324543a727549b6c2a6aad53db78f445e4f71668`.

## Unabhängige Replikation

Der unveränderte Gewinner wurde anschließend einmalig und lernfrei auf den
zuvor unberührten Seeds 60000–60049 geprüft:

- 35/50 Siege = 70 %,
- 0 Unentschieden und 15 Niederlagen,
- mittlere Token-Differenz: +6,50,
- mittlere Rundensiegrate: 60,63 %,
- mittlere Entscheidungszeit: 144,238 ms,
- vollständiger Modellzustand nach der Evaluation unverändert.

Die 70 % sind der stärkere Generalisierungsnachweis. Die 90 % des
Auswahl-Holdouts werden nicht als unabhängige Leistung interpretiert.

## Getrennte Gewinner-App

Der replizierte Snapshot wurde in
`Runenkrieg_Tatarus_Winner_Android` mit eigener Paket-ID übernommen.
Training, Moduswechsel und Zurücksetzen sind dort gesperrt. Asset-Hash,
Topologie, Trainingsstand und lernfreie Zustandsidentität wurden auf dem
Smartphone per Instrumentierungstest bestätigt.
