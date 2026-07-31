# Runenkrieg: TATARUS LargeScale

Experimenteller Android-Zweig von **TATARUS – A Persistent Synthetic
Nervous System**. Er untersucht, ob deutlich mehr neuronale Kapazität,
breitere Sensorik und vollständiges Mehr­runden-Training die Spielleistung
gegen regelbasierte und zufällige Gegner verbessern.

Der veröffentlichte Referenzstand bleibt unverändert in
[`../Runenkrieg_Tatarus`](../Runenkrieg_Tatarus). Diese Variante besitzt
eine eigene Android-Anwendungs-ID (`de.runenkrieg.game.large`) und kann
parallel zur Referenz-App installiert werden.

**Version:** 2.0.0-largescale  
**Entwickler:** Ralf Krümmel  
**Lizenz:** Apache License 2.0

> Größere Netze garantieren keine bessere Strategie. Die Überlegenheit ist
> eine zu testende Hypothese. Die App enthält deshalb identische Testseeds,
> Regel- und Zufallsbaselines sowie Mechanismusablationen.

## LargeScale-Profil

| Größe | Referenz | LargeScale |
|---|---:|---:|
| Neuronen | 72 | 1.024 |
| Rekurrente Synapsen | 432 | 32.768 |
| Eingabekanäle | 32 | 128 |
| Projektionen je Kanal | 1 | 8 |
| Afferente Projektionen | 32 | 1.024 |
| Neural-Bridge-Merkmale | 16 | 48 |
| maximale Assemblies | 16 | 64 |
| individuelle Axonverzögerung | 1–4 ms | 1–8 ms |

Der Kern enthält Dale-konforme E/I-Dynamik, passive Dendriten,
ereigniskausale Generated-Operator-Modulation, lokale signierte
Eligibility-Spuren, synaptische Depression und Facilitation,
Belohnungsmodulation, Konsolidierung, Energie, Homeostase und konkurrierende
Assemblies.

Die 128 Kanäle kodieren keine fertig entschiedene Aktion. Sie übertragen
unter anderem Kartenidentität, Fähigkeit, Kartentyp, Mechaniken, Wetter,
Handzusammensetzung, jüngere Spieler- und TATARUS-Sequenzen,
Rundenausgänge, Tokenlage, Heldenkontext und nichtlineare
Karten–Wetter-Interaktionen. Jeder Kanal beeinflusst den Kern direkt und
über sieben zusätzliche deterministische Projektionen.

## Was gegenüber der Referenz geändert wurde

- vollständige Partien statt isolierter Zufallsrunden im Selbsttraining,
- 80-dimensionaler Aktionsreadout aus 48 neuronalen und 32
  kandidatenbezogenen Merkmalen,
- flache Snapshot-Arrays statt eines Objekts je Synapse,
- atomare, gzip-komprimierte Dateipersistenz statt eines großen
  `SharedPreferences`-JSON,
- Messung der Entscheidungszeit auf dem Gerät,
- sichtbare Kapazitäts-, Zustands- und Eingangsmetriken im TATARUS-Labor,
- reduzierter Schnelltestumfang für interaktive Smartphone-Läufe.

## Bauen

Voraussetzungen: JDK 17, Android SDK 35, Android 8.0/API 26 oder neuer.

```powershell
$env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
.\gradlew.bat assembleDebug
```

APK:
`app/build/outputs/apk/debug/app-debug.apk`

## Tests

```powershell
$env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
.\gradlew.bat testDebugUnitTest --max-workers=1
.\gradlew.bat connectedDebugAndroidTest --max-workers=1
```

Die lokalen Tests prüfen alle 128 Eingabekanäle, die exakte
LargeScale-Kapazität, Determinismus, vollständige Snapshot-Rückkehr,
Dale-/Numerikinvarianten, Eligibility, Energie und Assemblybildung. Der
Gerätetest führt vollständige Partien in allen Forschungsmodi aus und prüft,
dass die lernfreie Evaluation das persistente Modell nicht verändert.

## Dokumentation

- [Technische LargeScale-Dokumentation](Tatarus_LargeScale_Dokumentation.md)
- [Vorregistriertes Vergleichsprotokoll](LARGESCALE_FORSCHUNGSPROTOKOLL.md)

