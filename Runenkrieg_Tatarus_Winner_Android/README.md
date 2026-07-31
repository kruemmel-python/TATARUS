# Runenkrieg – TATARUS Frozen Winner Android

Getrennte Android-Vergleichs-App für den nach
`RUNENKRIEG-TATARUS-MULTISEED-1` ausgewählten TATARUS-Gewinner.

## Eingefrorenes Modell

- Architektur: TATARUS LargeScale
- 1.024 Neuronen, 32.768 Synapsen, 128 Eingabekanäle
- Trainingsseed: `20260732`
- Auswahlcheckpoint: 10.000 Umweltrunden
- Auswahl-Holdout: 18/20 Siege (90 %), Token-Differenz +7,75
- unabhängige Replikation: 35/50 Siege (70 %), Token-Differenz +6,50
- Snapshot-SHA-256:
  `98c5671ebfcf64734d4d791e324543a727549b6c2a6aad53db78f445e4f71668`

Das gzip-komprimierte Modell wird als `tatarus_frozen_winner.snapshot`
beim ersten Start aus den APK-Assets in den privaten
App-Speicher importiert. Training, Moduswechsel und Zurücksetzen sind in
dieser Vergleichs-App gesperrt. Die neuronale Dynamik reagiert weiterhin
auf den Spielverlauf, aber Readout, Gewichte, Eligibility-Plastizität und
Assembly-Lernen bleiben deaktiviert.

Bei jedem neuen App-Prozess wird erneut der unveränderte eingebettete
Gewinner geladen. Dadurch können reale Testspiele innerhalb einer Sitzung
die laufende neuronale Dynamik beeinflussen, aber nicht unbemerkt den
Startzustand eines späteren Vergleichslaufs verändern.

## Reproduzierbarer Build

```powershell
$env:ANDROID_HOME='C:\Users\<NAME>\AppData\Local\Android\Sdk'
.\gradlew.bat testDebugUnitTest assembleDebug assembleDebugAndroidTest
```

Der Instrumentierungstest `FrozenWinnerAssetInstrumentedTest` prüft den
Asset-Hash, die Topologie, den 10.000-Runden-Trainingsstand und die
Zustandsidentität vor und nach einer lernfreien Evaluation.
