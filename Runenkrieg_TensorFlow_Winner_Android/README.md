# Runenkrieg – eingefrorener TensorFlow-Gewinner

Eigenständige Android-App für die mobile Replikation des konventionellen
Gewinners aus `RUNENKRIEG-TF-MULTISEED-1`.

Diese App ist bewusst getrennt von:

- `Runenkrieg_Tatarus`
- `Runenkrieg_Tatarus_LargeScale`
- `Runenkrieg_TensorFlow_Benchmark`

Sie hat die eigene Application-ID
`de.runenkrieg.game.tensorflowwinner`. Dadurch können TATARUS und der
konventionelle Gewinner gleichzeitig auf demselben Smartphone installiert
und unter derselben Hardware vermessen werden.

## Wissenschaftliche Funktion

Der Gegner ist nach der Desktop-Auswahl vollständig eingefroren:

- präregistrierter Gewinner: `contextual_bandit`,
- repräsentativer Trainingsseed: `20260731`,
- eingefrorener Checkpoint: `10.000` Umweltrunden,
- exakt 128 Eingabekanäle,
- derselbe variable legale Aktionsraum einschließlich Fusionen,
- Argmax-Auswahl ohne Exploration,
- keine Backpropagation auf Android,
- kein Replay Buffer,
- keine Gewichts- oder Optimizeränderung,
- lokale Zähler erfassen nur reale Spielrunden und Inferenzzeit.

Die App liest:

- `app/src/main/assets/runenkrieg_frozen_winner.tflite`
- `app/src/main/assets/winner_metadata.json`

Die Metadaten enthalten Architektur, Trainingsseed, Endcheckpoint,
Modellgröße, SHA-256-Prüfsumme und Ergebnis der unabhängigen
Desktop-Replikation.

## Build

```powershell
.\gradlew.bat testDebugUnitTest assembleDebug
```

Die Debug-APK entsteht unter:

`app/build/outputs/apk/debug/app-debug.apk`

Der verifizierte Build vom 31. Juli 2026 bestand Kotlin-Kompilierung,
Android-Unit-Tests und APK-Paketierung. Das APK enthält das Modell und
seine Metadaten als unkomprimierte Assets. Details und Prüfsummen stehen
in [`ANDROID_WINNER_PROTOCOL.md`](ANDROID_WINNER_PROTOCOL.md).

Die App verifiziert beim Start die SHA-256-Prüfsumme des Modells. Stimmen
Datei und Metadaten nicht überein, wird der Gegner nicht initialisiert.

## Wichtige Grenze

Das Android-Spiel misst den Transfer eines auf dem Python-Paritätsziel
trainierten konventionellen Modells in die echte Kotlin-App. Erst ein
Cross-Language-Goldentest des gesamten Spielkerns erlaubt eine streng
kontrollierte Aussage zur Architektur. Abweichungen sind deshalb sowohl
als Modellleistung als auch als Transferrobustheit zu berichten.
