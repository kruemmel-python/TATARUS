# TATARUS – A Persistent Synthetic Nervous System

**Developer:** Ralf Krümmel  
**License:** Apache License 2.0  
**Release:** 1.0.0

TATARUS ist ein persistentes synthetisches Nervensystem und eine
Forschungsplattform für ereignisgetriebene, biologisch inspirierte
Informationsverarbeitung. Der verbindliche Name des Projekts und Systems ist
**TATARUS – A Persistent Synthetic Nervous System**.

## Wissenschaftliches Unterprojekt: Runenkrieg × TATARUS

[`Runenkrieg_Tatarus`](Runenkrieg_Tatarus) überführt den persistenten
TATARUS-Kern in ein vollständig offline ausführbares Android-Kartenspiel.
Das Unterprojekt ist gleichzeitig:

- interaktive Agent–Umwelt-Umgebung,
- persistenter lernender Spielgegner,
- mobiles Trainingssystem,
- Messoberfläche für neuronale, synaptische und energetische Zustände,
- sowie Ablations- und Holdout-Labor mit identischen vollständigen
  Testpartien.

Der mobile Kern besitzt 72 Neuronen, 432 Dale-konforme Synapsen,
Axonverzögerungen, passive Dendriten, Generated-Operator-Modulation,
lokale signierte Eligibility-Spuren, kurzzeitige synaptische Dynamik,
Assemblies, Homeostase, Energie und einen begrenzten Aktionsreadout.
Alle 32 Spiel- und Kandidatenkanäle sind neuronalseitig verdrahtet.

Der Standardmodus **Reines TATARUS** entscheidet ohne Beimischung des
Regel- oder Aktionsmittelwerts. Regel-, Zufalls- und
Mechanismusablationen erlauben kausale Vergleiche. Die integrierte
Evaluation bestätigt technische Funktionsfähigkeit und
Reproduzierbarkeit; sie ist noch kein statistischer Nachweis strategischer
Überlegenheit.

Einstieg:

- [Android-Unterprojekt und Buildanleitung](Runenkrieg_Tatarus/README.md)
- [wissenschaftliches Whitepaper](Runenkrieg_Tatarus/Whitepaper_TATARUS_Runenkrieg_DE.md)
- [technische TATARUS-Runenkrieg-Dokumentation](Runenkrieg_Tatarus/Tatarus_Runenkrieg_Dokumentation.md)

### Experimenteller LargeScale-Zweig

[`Runenkrieg_Tatarus_LargeScale`](Runenkrieg_Tatarus_LargeScale) bewahrt
die veröffentlichte 72/432/32-Fassung als Referenz und untersucht separat
eine mobile Skalierung auf 1.024 Neuronen, 32.768 rekurrente Synapsen,
128 vollständig verdrahtete Eingabekanäle und 1.024 afferente
Eingangsprojektionen. Der Zweig besitzt eine eigene Android-App-ID, flache
Snapshots, gzip-Dateipersistenz, einen 80-dimensionalen Readout und
vollständiges Mehr­runden-Selbsttraining.

Die strategische Überlegenheit ist noch keine bestätigte Tatsache. Das
[vorregistrierte Protokoll](Runenkrieg_Tatarus_LargeScale/LARGESCALE_FORSCHUNGSPROTOKOLL.md)
definiert Holdout-, Baseline- und Replikationsbedingungen, unter denen eine
solche Aussage geprüft werden darf.

### TensorFlow-Kontrollarchitekturen

[`Runenkrieg_TensorFlow_Benchmark`](Runenkrieg_TensorFlow_Benchmark)
stellt TATARUS unter identischen 128-Kanal-, Aktions-, Reward- und
Seedbedingungen einem MLP, einer GRU, DQN, PPO und einem Contextual Bandit
gegenüber. Das Labor misst Lernkurven bei 250 bis 10.000 beobachteten
Umweltrunden, Entscheidungslatenz, CPU-/Parameterspeicher, Retention,
Geschichtsabhängigkeit und Anpassung nach einem unangekündigten
Regelwechsel.

### Symmetrischer TATARUS-10k-Benchmark

[`Runenkrieg_Tatarus_10k_Benchmark`](Runenkrieg_Tatarus_10k_Benchmark)
trainiert fünf unabhängige TATARUS-LargeScale-Modelle unter demselben
Checkpoint- und Holdoutschema bis 10.000 Runden. Erst der
vorregistriert ausgewählte und anschließend unabhängig replizierte
TATARUS-Snapshot darf in die getrennte Vergleichs-APK übernommen werden.

Der Lauf ist mit 30 von 30 Checkpoints abgeschlossen. Die mittlere
Partiensiegrate der fünf Modelle beträgt am 10.000er-Punkt 81 %
(95-%-Bootstrapintervall 75–86 %). Seed `20260732` wurde vorregistriert
ausgewählt und erzielte auf den unberührten Replikationsseeds 60000–60049
35/50 Siege = 70 %. Der vollständige
[Laufstatus](Runenkrieg_Tatarus_10k_Benchmark/RUN_STATUS.md) trennt
Auswahl-Holdout und unabhängige Replikation ausdrücklich.

## Bestätigtes persistentes Endsystem

Forschungsstufe 16 ergänzt das bisherige Versuchsnetz um einen dauerhaft
fortgesetzten C++-Nervensystemkern, eine eigene Labor-UI, vollständige
Snapshots, multimodale Rohsensorik, Closed-Loop-Handlung, Assembly-Bildung,
Neuromodulation, Energie-/Homeostaseregelung, Strukturplastizität,
Parameter-Evolution und eine validierte Mechanismenbibliothek.

Start:

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\run_nervous_system_ui.bat
```

Architektur, Messergebnisse und wissenschaftliche Grenzen stehen in
`research/ag_signal_morpher_1ee27305a6aa/16_persistent_nervous_system/README.md`.

Die anschließenden kontrollierten Prüfungen zu selbstständiger
Repräsentationsbildung, tokenizerfreien Sequenzen, reizfreiem Recall und
Funktionswiederherstellung stehen in
`research/ag_signal_morpher_1ee27305a6aa/17_autonomous_representation/README.md`.

Forschungsstufe 18 hat die vier hierfür eingefrorenen Endzielkriterien auf
unberührten Seeds bestanden: Repräsentationen 8/8, rohe Sequenzen 8/8,
Trace-essential Recall 100 % gegenüber 48,6111 % ohne Spur sowie
Funktionsreparatur mit Pfadprovenienz 8/8. Bericht und Rohdaten liegen unter
`research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp/stage18_release_confirmation`.
Die Bestätigung ist auf diese synthetischen Aufgaben begrenzt und keine
Behauptung biologischer Gleichwertigkeit oder allgemeiner Intelligenz.

Forschungsstufe 19 koppelt das System über eine beschränkte
`CognitiveBridge` an einen höheren Planungskern. In einem durchgängigen
Lebenslauf ohne Sample-Reset erreicht die gekoppelte KI auf 8/8 neuen Seeds
100 % Handlungsaccuracy; ohne lokale Nervenspur sind es 51,5625 %, ohne
Nervensystem 50 %. Komposite Snapshots setzen Nervenzustand, Recall und
Handlung exakt fort. Details:
`research/ag_signal_morpher_1ee27305a6aa/19_persistent_ai_bridge/README.md`.

Die kostensparend zusammengefassten Stufen 20–23 ergänzen eine prozedurale
offene Lebenswelt mit G5-Transfer, episodisches/konsolidiertes/prozedurales
Gedächtnis und kontrolliertes Vergessen, Sparse-Skalierung bis 65.536
Neuronen sowie ein portables Clean-Build-Replikationspaket. Stufe 20 bestand
6/8, Stufe 21 8/8 neue Seeds. Details:
`research/ag_signal_morpher_1ee27305a6aa/20_23_validation/README.md`.

Dieses Projekt integriert den unveränderten Algorithmic-Genesis-Kernel
`ag_signal_morpher_1ee27305a6aa` als experimentellen Modulator synaptischer
Wirksamkeit in ein deterministisches Leaky-Integrate-and-Fire-Netzwerk.

Das Modell enthält:

- exzitatorische und inhibitorische Neuronen nach dem Dale-Prinzip,
- Membranleck, Reset, Refraktärzeit und adaptive Feuerschwelle,
- spärliche rekurrente Verbindungen,
- optionale lokale spike-timing-abhängige Plastizität (STDP),
- sechs Gate-Modi und event-konditionierte Pflichtkontrollen,
- kausale Spikeereignisse mit wählbarer pre-reset Spannung oder E/I-Balance,
- eine parametrierbare Vier-Feature-Projektion aus E/I-Balance,
  Membransteigung, Schwellenüberschuss und Inter-Spike-Intervall,
- editierbare Membran-, Synapsen-, Adaptations-, STDP- und Readoutparameter,
- individuelle Axonverzögerungen,
- wahlweise strom- oder AMPA-/GABA-leitwertbasierte Synapsen,
- ein optionales passives Dendritenkompartiment,
- getrennte Operatoren für `E→E`, `E→I`, `I→E` und `I→I`,
- Mehrseed-Auswertung mit gepaarten Permutationstests,
- Holdout-Optimierung der Projektionsgewichte,
- reproduzierbare Charakterisierungs-, Backend- und Ablationsexperimente,
- fünf automatisierte Forschungs-Referenzzustände,
- eine zeitliche Musterklassifikation mit stratifizierter Cross-Validation,
- eine native C++/Win32-Oberfläche mit Spike-Raster, Spannungsverlauf,
  Gate-Vergleich und Berichtsexport.

Es handelt sich um ein synthetisches Forschungsmodell, nicht um ein validiertes
Modell eines realen biologischen Systems.

## Ausführen

```powershell
$env:PYTHONPATH = "research\ag_signal_morpher_1ee27305a6aa\05_integration\source"
python -m biological_neural_network --steps 300 --seed 38
python -m biological_neural_network --timing emission_state
python -m biological_neural_network --timing emission_state --emission-feature ei_balance
python -m biological_neural_network --timing emission_state --emission-feature feature_projection
python -m biological_neural_network --gate-control time_shifted
python -m unittest discover -s research/ag_signal_morpher_1ee27305a6aa/05_integration/tests -v
python research/ag_signal_morpher_1ee27305a6aa/04_experiments/run_research.py
```

Alternativ kann das Projekt lokal installiert werden:

```powershell
python -m pip install -e .
```

Der Forschungs- und Testlauf funktioniert auch ohne Installation direkt aus
der Projektwurzel.

## Native C++-Oberfläche

Die Oberfläche wurde bereits unter
`research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\build`
kompiliert. Start:

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\run_ui.bat
```

Neu bauen und anschließend starten:

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\build_ui.bat
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\run_ui.bat
```

In der UI können Neuronenzahl, Simulationsschritte, Seed, Rauschen,
Pulshöhe, Verbindungsdichte, konstantes Gate, Stichprobenzahl, Folds,
Gate-Modus, `RESET_LOCKED`/`EMISSION_STATE`, zeitliche bzw. state-shuffled
Kontrollen, Emissionsfeature und STDP präzise eingestellt werden.
Semikolonfelder besitzen Gültigkeitsmarker und Tooltips mit dem vollständigen
aktuellen Wert. Vor jedem Lauf zeigt die UI einen Konfigurationshash; der
Bericht enthält die geparste Konfiguration. Ausgegeben werden globale und
tatsächlich wirksame Gatewerte, Gateentropie,
Eventfeature-Statistik, Assembly-Separation, Spikes je korrekter Entscheidung
sowie Fano-, Korrelation- und Koinzidenzmetriken.

Die vollständige Beschreibung aller Felder, Wechselwirkungen, Metriken,
Festwerte und empfohlenen Versuchsabläufe steht in `UI_DOKUMENTATION.md`.

Die aktuelle ereigniskausale E/I-Stufe ist wirklich dynamisch, hat in der
ersten Reihenfolgeerkennung aber noch keinen Vorteil gegenüber der
event-gematchten Konstante gezeigt. Die Resultate stehen unter
`research\ag_signal_morpher_1ee27305a6aa\08_event_causal_ei`.

Die nächste Stufe mit Vier-Feature-Projektion und Komponentenablationen steht
unter `research\ag_signal_morpher_1ee27305a6aa\09_feature_projection`.

Die technische Abnahme der erweiterten Biophysik und Operatorökologie:

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\build\AGBioNetworkAcceptance.exe
```

Der aktuelle Messbericht liegt unter
`research\ag_signal_morpher_1ee27305a6aa\10_acceptance`.

## Bestätigender 24-Seed-Lauf

Der vorab festgelegte Lauf über 24 neue Seeds und vier Belastungsbedingungen
bestätigt keine Accuracy-Überlegenheit. Er bestätigt jedoch bei
nichtunterlegener Accuracy einen Holm-korrigierten Spikekostenvorteil des
Originalkernels gegenüber event-gematchter Konstante, deaktiviertem Gate und
verteilungsgematchtem Zufallsgate. Das Vorzeichengate bleibt geringfügig
sparsamer; eine allgemeine Überlegenheit wird daher nicht behauptet.

Ausführung:

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\build\AGBioNetworkSuperiority.exe research\ag_signal_morpher_1ee27305a6aa\11_superiority_multiseed
```

Plan, Rohdaten und Statistik stehen unter
`research\ag_signal_morpher_1ee27305a6aa\11_superiority_multiseed`.

## Delayed-XOR-Replikation

Die unabhängige Replikation auf Delayed XOR mit 24 weiteren Seeds und zwei
Gedächtnislücken war negativ. Alle Gatevarianten erreichten nur ungefähr
`51 %` Accuracy. Der Kernel war gegenüber Konstante, Zufall und dem
Vorzeichengate nicht sparsamer. Die vorherige Effizienzbehauptung darf daher
nicht auf Delayed XOR verallgemeinert werden.

Ausführung:

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\build\AGBioNetworkDelayedXor.exe research\ag_signal_morpher_1ee27305a6aa\12_delayed_xor_replication
```

Der vorab festgelegte Plan, 288 Rohdatensätze und der Bericht liegen unter
`research\ag_signal_morpher_1ee27305a6aa\12_delayed_xor_replication`.

## Delayed XOR zuverlässig gelernt

Auf den bereits verbrauchten XOR-Seeds wurde anschließend ein kausaler
Gedächtnisreadout entwickelt: lange exponentielle Spike-Traces,
Soma-/Dendritzustände und eine cue-gebundene Eligibility-Memory. Nach
Erreichen von `90,63 %` Entwicklungsaccuracy wurde das Modell mit Hash
`EECE7A502A958561` eingefroren.

Auf 16 vorher unangetasteten Seeds erreicht es:

```text
Accuracy                  0.892578
untere einseitige Grenze  0.876953
mittlere Verzögerung      0.871094
lange Verzögerung         0.914062
```

Delayed XOR wird damit zuverlässig gelernt. Die frühere
Operator-Effizienzüberlegenheit repliziert sich trotzdem nicht; das
Vorzeichengate bleibt geringfügig sparsamer.

Details stehen unter
`research\ag_signal_morpher_1ee27305a6aa\13_memory_readout_development`.

Eligibility-Zeiten, Eligibility-Memory und Interaktionsprodukte sind jetzt
in der nativen UI verfügbar. **Delayed-XOR Einzelablationen** vergleicht
Vollmodell, Dendrit aus, Eligibility aus, Produkte aus und Vorzeichengate auf
identischen Seeds und zeigt den erzeugten Bericht direkt in der UI.
## Runenkrieg-Vergleichsexperimente

- `Runenkrieg_Tatarus_LargeScale`: skaliertes persistentes TATARUS-System
  mit 1.024 Neuronen, 32.768 Synapsen und 128 Eingabekanälen.
- `Runenkrieg_TensorFlow_Benchmark`: vorregistrierte Mehrseed-Lernkurven
  für MLP, GRU, DQN, PPO und Contextual Bandit bis 10.000 Runden.
- `Runenkrieg_TensorFlow_Winner_Android`: getrennte Android-App für den
  eingefrorenen konventionellen Gewinner.
- `Runenkrieg_Tatarus_10k_Benchmark`: vorregistrierter, gerätenativer
  Gegenlauf für fünf unabhängige TATARUS-Modelle; 30/30 Checkpoints und
  unabhängige Replikation abgeschlossen.
- `Runenkrieg_Tatarus_Winner_Android`: getrennte Android-App mit dem
  eingefrorenen und hash-identifizierten TATARUS-Gewinner aus 10.000 Runden.

Die gemeinsame, bewusst zurückhaltende Einordnung steht in
[`RUNENKRIEG_VERGLEICHSBERICHT.md`](RUNENKRIEG_VERGLEICHSBERICHT.md).
