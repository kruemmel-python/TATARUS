# Forschungsstufe 17/18 – autonome Repräsentation, Gedächtnis und Reparatur

Status: `confirmed_on_synthetic_holdouts`  
Stand: 29. Juli 2026

## Ergebnis

Die in Stufe 17 zunächst offenen Hypothesen wurden mechanistisch
weiterentwickelt und anschließend mit vorab eingefrorenen Kriterien auf
unberührten Seeds geprüft. Der maßgebliche Endlauf liegt unter:

`06_temporal_classification/ui_cpp/stage18_release_confirmation`

| Hypothese | unabhängiges Ergebnis | Status |
|---|---:|---|
| stabile konkurrierende Repräsentationen | 8/8 Seeds | bestätigt |
| tokenizerfreie Übergänge und Grenzen | 8/8 Seeds | bestätigt |
| Trace-essential Recall-XOR | 100 % gegenüber 48,6111 % ohne Spur auf 12 Holdout-Netzen | bestätigt |
| Funktionsreparatur mit Pfadprovenienz | 8/8 Seeds | bestätigt |

Die Bestätigung gilt für die definierten synthetischen Aufgaben. Sie beweist
weder biologische Gleichwertigkeit noch Bewusstsein oder allgemeine
Intelligenz.

## Implementierte Mechanismen

### Kompetitive zeitliche Assemblies

Reizantworten werden in einer festen kausalen Reizphase als signed evoked
state gegenüber dem Zustand vor dem Reiz erfasst. Der ähnlichste Prototyp
gewinnt; liegt seine Kosinusähnlichkeit unter der konfigurierten Schwelle,
entsteht eine neue Assembly. Dadurch fallen verschiedene Reize nicht mehr in
einen einzigen globalen Prototyp.

Im Holdout entstanden im Mittel 6,125 Assemblies. Ähnliche Reize
reaktivierten ihre Repräsentation mit 0,907323; nach 10/15-%-Schaden blieb
eine Ähnlichkeit von 0,852185 erhalten.

### Tokenizerfreie Sequenzbildung

UTF-8-Bytes werden unverändert als bitweise Ereignisse eingespeist. Es gibt
keine Token-, Wort- oder Embeddingtabelle. Ein nur auf Trainingsepochen
angepasster linearer Readout erkannte sechs Übergangsklassen in unberührten
Epochen mit im Mittel 77,3438 %. Die Grenzreaktion war um Faktor 2,884757
stärker als die normale Übergangsreaktion.

### Trace-essential Memory

Zwei frühe Cues tragen XOR-Information, danach folgen 400 vollständig
reizfreie Schritte und derselbe neutrale Recall-Cue für alle Klassen. Das
Readout sieht ausschließlich Spikeänderungen und Endmembranzustände im
letzten Recallfenster. Cue-Merkmale, Eligibility-Werte und Produktfeatures
werden ihm nicht gegeben.

Der lokale, signierte Zustand jeder Synapse zerfällt exponentiell und
moduliert spätere Übertragung. Tau, Gain und Inkrement wurden nur auf
Entwicklungsseeds gewählt (`800 ms`, `10`, `20`). Auf zwölf getrennten
Holdout-Netzen erreichte das Spurenmodell 1,0 Accuracy, die identische
Architektur ohne Eligibility 0,486111.

### Kausale Funktionswiederherstellung

Vor dem Schaden wird eine tatsächlich benutzte Sensor-Motor-Funktion
etabliert. Danach werden exakt ihre sechs direkten Leitungen deaktiviert und
zusätzlich 10 % interne Neuronen beschädigt. Die Funktion fällt in allen
acht Holdout-Netzen auf null.

Strukturplastizität darf Ersatzsynapsen nur aus zuvor benutzten, konsolidierten
und nun inaktiven Pfaden erzeugen. Jede Ersatzsynapse speichert den Index
ihres Elternpfads. Nach dem Reparaturfenster werden Strukturwachstum und
Homeostasedrift eingefroren. In 8/8 Netzen entstanden sechs
provenienztragende Ersatzsynapsen; im Mittel wurden 111,3726 % des
ursprünglichen Funktionsbetrags mit gleichem Vorzeichen wiedergewonnen.

## Reproduzieren

```powershell
cd research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp
build_endgoal\AGRepresentationResearch.exe stage18_confirmation --confirm
```

Ausgaben:

- `STAGE18_CONFIRMATION.md`: lesbarer Bestätigungsbericht,
- `stage18_confirmation.json`: maschinenlesbarer Gesamtstatus,
- `confirmation_seeds.csv`: Rohwerte aller acht Seeds,
- `.agns`: V9-Snapshots vor Schaden, unmittelbar danach und nach Reparatur.

Die Schaltfläche **Endziel bestätigen** in `TATARUS.exe` startet
denselben eingefrorenen Lauf.
