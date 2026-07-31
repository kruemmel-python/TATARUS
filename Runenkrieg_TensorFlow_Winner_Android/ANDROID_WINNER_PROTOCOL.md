# Protokoll des eingefrorenen Android-Gegners

## Provenienz

Der Modellinhalt darf ausschließlich durch folgenden Ablauf ersetzt
werden:

1. vollständiger Fünfseed-Lauf bis 10.000 Runden,
2. Auswahl nach der vorregistrierten Regel in
   `Runenkrieg_TensorFlow_Benchmark/EXPERIMENT_PROTOCOL.md`,
3. Prüfung auf den unberührten Seeds 60000 bis 60049,
4. Export mit `runenkrieg_tf.freeze_winner`,
5. Kopie von Modell und unveränderten Metadaten in die Android-Assets,
6. SHA-256-Abgleich vor dem Build.

Ein Modell aus einem Smoke-Test ist kein gültiger Releasekandidat.

## Herkunft und Ergebnis

- Desktop-Protokoll: `RUNENKRIEG-TF-MULTISEED-1`
- Architekturen: MLP, GRU, DQN, PPO und Contextual Bandit
- Trainingsseeds: `20260730` bis `20260734`
- Lernkurvenpunkte: 250, 500, 1.000, 2.000, 5.000 und 10.000 Runden
- präregistrierter Gewinner: `contextual_bandit`
- repräsentativer Trainingsseed: `20260731`
- Auswahlcheckpoint: `10.000`

Der Contextual Bandit erreichte am Endcheckpoint über fünf
Trainingsseeds im Mittel 65 % Partiensiege. Das 95-%-Bootstrapintervall
lag bei 61–69 %. In der anschließenden unabhängigen Replikation auf 50
unberührten Seeds erreichte das eingefrorene Modell:

- 60 % Partiensiege,
- 53,3 % Rundensiege,
- +2,52 mittlere Token-Differenz,
- 0,010 ms mittlere Desktop-Entscheidungszeit.

Die Unterschiede zu den vier anderen Architekturen waren nach
Holm-Korrektur nicht statistisch signifikant. „Gewinner“ bezeichnet
daher ausschließlich die vorregistrierte Auswahl innerhalb dieses
Versuchs.

## Entscheidungspfad

```text
Kotlin-Spielzustand
    -> alle legalen Einzelkarten und Fusionen
    -> 128 Rohkanäle je Kandidat
    -> eingefrorenes LiteRT-Modell
    -> ein Score je Kandidat
    -> deterministisches Argmax
    -> normale Kotlin-Regelauswertung
```

Bei einem GRU-Gewinner erhält jeder Kandidat sieben frühere ausgewählte
128-Kanal-Vektoren plus den aktuellen Vektor. Die Historie wird zu Beginn
jeder Partie gelöscht. Bei allen anderen Gewinnern wird ausschließlich
der aktuelle Kandidat übergeben.

## Unveränderlicher Modellimport

- Modell: `runenkrieg_frozen_winner.tflite`
- Modellgröße: 1.504 Byte
- SHA-256:
  `e94827bd1a09120e8fe4ec531af9da9a2418971b570c3804b1a6de68f7510e8e`
- maximale Exportabweichung Desktop zu LiteRT:
  `5.960464477539063e-08`
- Select-TF-Operationen: nein
- Lernen auf Android: nein

Eine separate Importprüfung bestätigt Hash, Größe, Byteidentität und
Metadatenidentität zwischen Desktop-Export und Android-Assets. Zusätzlich
prüft die Android-App die Modellprüfsumme vor dem Initialisieren der
LiteRT-Laufzeit.

## Unveränderlichkeitsregeln

- `learn()` protokolliert nur reale Resultate und aktualisiert bei GRU die
  flüchtige Eingabesequenz.
- Kein Tensor, Gewicht oder Optimizerzustand wird gespeichert.
- `train()` führt keine Trainingsoperation aus.
- Die Modellauswahl ist deterministisch.
- Das Modellasset wird bei einem Statistik-Reset nicht verändert.

## Verifizierter Android-Build

- Application-ID: `de.runenkrieg.game.tensorflowwinner`
- Version: `1.0.0-frozen-winner`
- minSdk: 26
- targetSdk: 35
- LiteRT: 2.1.6
- APK-Größe: 38.760.705 Byte
- Debug-APK-SHA-256:
  `5f1f05c4e010f20b1d47764b27d71aac003821722d073fc3d99885d8478b1b2a`
- Build: erfolgreich
- Android-Unit-Tests: 5 erfolgreich, 0 Fehler

Der Build wurde isoliert mit deaktiviertem inkrementellem Kotlin-Cache
ausgeführt:

```powershell
.\gradlew.bat clean testDebugUnitTest assembleDebug `
  --no-daemon --max-workers=1 `
  -Pkotlin.incremental=false `
  -Pkotlin.compiler.execution.strategy=in-process
```

## Installationstest auf realer Hardware

Am 31. Juli 2026 wurde die Debug-APK auf einem angeschlossenen
`RMX3853` installiert und als eigener Prozess gestartet:

- ADB-Installation: erfolgreich,
- Kaltstartstatus: `ok`,
- gemessene Android-Startzeit: 1.620 ms,
- Prozess fünf Sekunden nach dem Start aktiv,
- keine `FATAL EXCEPTION` im gefilterten Startprotokoll.

Dies belegt den technischen Import und Start auf realer Hardware. Es ist
noch keine mobile Leistungsevaluation und wird nicht als Sieg gegen
TATARUS gewertet.

## Mobile Vergleichsmessung

TATARUS LargeScale und der eingefrorene Gewinner müssen auf demselben
Gerät nacheinander getestet werden. Zu protokollieren sind mindestens:

- Partie- und Rundensiegrate,
- Token-Differenz,
- Entscheidungen pro Partie,
- mittlere und p95-Inferenzzeit,
- Prozessspeicher,
- CPU-Zeit und Energie unter demselben Messwerkzeug,
- Leistung vor und nach einer Pause,
- Verhalten nach einem vorab festgelegten Regelwechsel.

Trainingseffizienz und mobile Ausführungseffizienz sind getrennte
Fragestellungen und dürfen nicht zu einer einzigen Kennzahl vermischt
werden. Eine allgemeine Überlegenheit gegenüber TATARUS darf erst aus
einer gemeinsamen, seed- und gerätegleichen Evaluation abgeleitet werden.
