# Forschungsstufe 19 – Persistent AI Nervous System

Status: `confirmed_on_synthetic_holdouts`  
Stand: 29. Juli 2026

## Ziel

Diese Stufe koppelt den persistenten Nervensystemkern erstmals über eine
beschränkte Funktionsschnittstelle an einen höheren Planungskern. Der höhere
Kern kann keine einzelnen Neuronen, Synapsen, Gewichte oder
Eligibility-Werte lesen oder verändern.

Er erhält ausschließlich:

- aktive Repräsentationen,
- gepoolte Recall-Zustände,
- Neuheit und Salienz,
- Energie- und Aktivitätsbedarf,
- Vorhersagefehler und Handlungskonfidenz.

Zurückgeben darf er ausschließlich:

- Aufmerksamkeitsziel,
- motorische Absicht,
- Recall-Cue,
- Konsequenz beziehungsweise Reward.

Die öffentliche API steht in `cognitive_bridge.hpp`.

## Persistent AI Nervous System Trial

Alle 64 Erfahrungen eines Seeds laufen ohne Systemreset in einem einzigen
Nervensystem. Zwei frühe energiegleiche Ereignisse erscheinen als
multimodale Reihenfolge `A→B` oder `B→A`. Nach einer reizfreien Phase folgt
für beide Klassen derselbe Recall. Nur die Vorgeschichte bestimmt, welche
Handlung richtig ist.

Nach dem Lernen wird eine unbekannte rohe Bytegrammatik eingespielt. Danach
muss die frühere Handlungsfunktion weiterbestehen. Der höhere Kern lernt
kontinuierlich aus Konsequenzen, besitzt aber keinen eigenen Cue-Puffer.

Ein kompositer Checkpoint speichert:

- vollständiges V9-Nervensystem,
- Zustand der Cognitive Bridge,
- Parameter des höheren Planungskerns.

Fortsetzung und Replay müssen bitgenau dieselben Nervenzustände,
Recall-Features und Handlungen erzeugen.

## Unabhängige Bestätigung

| Variante | mittlere Accuracy |
|---|---:|
| höhere KI mit persistentem Nervensystem | 1,000000 |
| identische Kopplung ohne lokale Eligibility | 0,515625 |
| höhere KI ohne Nervensystem | 0,500000 |

Alle acht neuen Seeds erfüllten einzeln die vorab festgelegten Kriterien.
Die erfahrungsabhängige Aktionsdiversität betrug 1,0; beide
Handlungsrichtungen wurden beim identischen aktuellen Recall aufgrund der
unterschiedlichen Vergangenheit benutzt. Sämtliche Snapshot-Replays waren
exakt.

Rohbericht:

`../06_temporal_classification/ui_cpp/stage19_final_release`

## Ausführen

```powershell
cd research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp
build_persistent_ai_trial.bat build_stage19
build_stage19\AGPersistentAITrial.exe stage19_confirmation --confirm
```

Alternativ startet die Schaltfläche **KI-Kopplung testen** in der
Nervensystem-UI denselben Acht-Seed-Lauf.

## Aussagegrenze

Bestätigt ist eine kausale, persistente und handlungswirksame Kopplung in
dieser teilweise beobachtbaren synthetischen Lebenslaufaufgabe. Noch nicht
bestätigt sind eine offene reale Umwelt, allgemeine Grammatikübertragung,
vollständige Langzeitkonsolidierung, produktive Großskalierung oder
allgemeine Intelligenz.
