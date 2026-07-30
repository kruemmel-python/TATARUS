# Runenkrieg mit TATARUS – Android

Native Android-Version des Runenkrieg-Kartenspiels. Der bisherige
Contextual-Bandit-Gegner wurde durch **TATARUS – A Persistent Synthetic
Nervous System** ersetzt. Das Spiel arbeitet vollständig offline.

Dieses Verzeichnis ist das wissenschaftliche Android-Unterprojekt des
Hauptprojekts
[TATARUS – A Persistent Synthetic Nervous System](https://github.com/kruemmel-python/TATARUS).
Es verbindet eine spielbare Anwendung mit einem persistenten
Spiking-Gegner, einer Forschungsoberfläche, kausalen
Mechanismusablationen und reproduzierbaren vollständigen Testpartien.

**Version:** 1.4.0
**Entwickler:** Ralf Krümmel
**Lizenz:** Apache License 2.0, entsprechend der Lizenz im Repository-Stamm

Die vollständige technische und funktionale Beschreibung befindet sich in
[Tatarus_Runenkrieg_Dokumentation.md](Tatarus_Runenkrieg_Dokumentation.md).

Das wissenschaftliche Whitepaper zur Motivation, Spielintegration und
Funktion des Android-Spiels als Forschungslabor befindet sich in
[Whitepaper_TATARUS_Runenkrieg_DE.md](Whitepaper_TATARUS_Runenkrieg_DE.md).

## Enthalten

- Vollständiges Kartendeck aus 10 Elementen × 14 Fähigkeiten
- Helden-, Wetter-, Moral-, Element- und Synergieboni
- Alle sieben Mechaniken: Ketteneffekte, Elementarresonanz, Überladung,
  Fusion, Wetterbindung, Verbündeter und Segen/Fluch
- Fusion für Spieler und TATARUS
- Persistenter, ereignisgetriebener TATARUS-Gegner
- Automatisches Grundtraining beim ersten Start
- Zusätzliches Offline-Selbsttraining im TATARUS-Labor
- Siebenseitiges Spieler-Handbuch direkt in der App
- Keine Internetberechtigung, kein Konto und kein API-Schlüssel erforderlich

## TATARUS-Gegner

Die Android-Integration verwendet einen für Mobilgeräte dimensionierten
TATARUS-Kern mit 72 Neuronen und 432 gerichteten Synapsen. Sie übernimmt
zentrale Mechanismen aus dem Forschungsprojekt:

- vollständige neuronale Verdrahtung aller 32 Spiel- und Kandidatenkanäle
- reines TATARUS als Standardmodus ohne Regel- oder Erfahrungsbeimischung
- Pflichtbaselines und lernfreie vollständige Mehrseed-Testspiele
- getrennte Real-, Trainings-, Assembly-, Eligibility- und Energiemetriken
- rekurrente, Dale-konforme exzitatorische und inhibitorische Dynamik
- passive dendritische Zustände und individuelle Axonverzögerungen
- ereigniskausale Übertragung durch den exportierten Generated Operator
- lokale vorzeichenbehaftete Eligibility-Spuren
- synaptische Depression und Facilitation
- Belohnungsmodulation und langsame Konsolidierung
- Energiehaushalt, Homeostase und adaptive Schwellen
- konkurrierende Assemblies und einen begrenzten neuronalen Readout

Vor jeder Antwort verarbeitet TATARUS den aktuellen Spielkontext. Legale
Einzelkarten und Fusionen werden danach in kurzen, rücksetzbaren neuronalen
Rollouts verglichen. Nur die gewählte Handlung wird in den dauerhaften
Nervenzustand übernommen. Nach dem Rundenergebnis verändern Belohnung und
lokale Eligibility-Spuren die Synapsen und den Aktionsreadout.

Der Zustand wird nach realen Runden in Android `SharedPreferences`
gespeichert und beim nächsten App-Start fortgesetzt. Das TATARUS-Labor zeigt
neben Spielbeobachtungen auch neuronale Schritte, Spikes, Synapsen,
Assembly-Entropie und -Trennung, Energieverteilung sowie die vollständige
Eligibility-Verteilung. Dort können außerdem Pflichtbaselines auf
identischen lernfreien Testspielen verglichen werden.

> Diese Integration ist ein nativer, verkleinerter Gameplay-Adapter des
> TATARUS-Forschungssystems. Sie bindet nicht die Windows-Forschungsoberfläche
> oder deren kompletten Experimentkatalog in die Android-App ein.

## Spieler-Handbuch

Über **Handbuch** öffnet sich eine offline verfügbare, für Smartphone und
Tablet skalierende Anleitung. Sie enthält Spielziel, Rundenablauf,
Stärkeformel, Wetter, Elementkonter, Tokeneffekte, Mechaniken, Fusion und
eine Strategie gegen TATARUS.

## Voraussetzungen

- Android Studio mit JDK 17
- Android SDK 35
- Mindestens Android 8.0 / API 26

## Starten

1. Den Ordner `Android_Game` in Android Studio öffnen.
2. Gradle synchronisieren lassen.
3. Ein Gerät oder einen Emulator ab Android 8.0 auswählen.
4. Die Konfiguration `app` starten.

Alternativ unter Windows:

```powershell
.\gradlew.bat assembleDebug
```

Das Debug-APK liegt danach unter
`app/build/outputs/apk/debug/app-debug.apk`.

## Tests

```powershell
.\gradlew.bat testDebugUnitTest lintDebug
.\gradlew.bat connectedDebugAndroidTest
```

Die Tests prüfen die Spielregeln sowie Determinismus, Snapshot-Rückkehr und
numerische/Dale-Invarianten des mobilen TATARUS-Kerns. Der Gerätetest spielt
alle Forschungsmodi auf vollständigen Partien und bestätigt, dass die
Evaluation den persistenten Zustand anschließend exakt wiederherstellt.
