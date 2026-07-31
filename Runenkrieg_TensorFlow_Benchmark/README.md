# Runenkrieg TensorFlow Benchmark

Konventionelles Vergleichslabor für **TATARUS – A Persistent Synthetic
Nervous System**. Dieser Ordner verändert weder
`Runenkrieg_Tatarus` noch `Runenkrieg_Tatarus_LargeScale`.

Das Labor spielt dieselbe Runenkrieg-Umwelt mit fünf üblichen
Machine-Learning-Verfahren:

| Agent | Architektur | Lernverfahren |
|---|---|---|
| MLP | Dense 128 → 128 → 64 → 1 | Backpropagation, Adam |
| GRU | 8 × 128 → GRU-64 → Dense | BPTT, Adam |
| DQN | Dense-Q-Netz plus Target-Netz | Replay Buffer, TD-Lernen |
| PPO | Dense-Policy-Scorer | geclippte On-policy-Aktualisierung |
| Contextual Bandit | linearer 128-Kanal-Scorer | inkrementelles Reward-Lernen |

TATARUS LargeScale ist die sechste Architektur und wird über die gleiche
Messwerttabelle ergänzt.

## Fairness

- gleicher 128-dimensionaler Rohzustand,
- keine fertig berechneten Element- oder Wettervorteile im Eingang,
- gleicher variable legaler Aktionsraum einschließlich Fusionen,
- gleiche Spiel-, Wetter- und Gegnerseeds,
- gleiche Zahl tatsächlich beobachteter Umweltrunden,
- Exploration nur im Training,
- Holdout ohne Gewichtsänderung,
- Messpunkte 250, 500, 1.000, 2.000, 5.000 und 10.000 Runden.

Der Python-Port enthält Deck, Helden, Wetter, Elementhierarchie, Synergien,
Fusionen, Tokenfolgen und alle sieben Mechaniken. Vor einer Veröffentlichung
ist zusätzlich ein Cross-Language-Goldentest gegen die Kotlin-App
vorgeschrieben.

## Installation

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
```

TensorFlow 2.21 wird als echtes Trainingsframework verwendet. LiteRT ist
für einen späteren Android-Export vorgesehen; das Benchmarktraining bleibt
bewusst headless, damit sämtliche Architekturen unter exakt denselben
Experimentseeds laufen.

## Schnelltest

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.experiment --quick
```

## Vollständiger Versuch

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.experiment `
  --agents mlp gru dqn ppo contextual_bandit `
  --seeds 20260730 20260731 20260732 20260733 20260734 `
  --output results_full
```

Ausgaben:

- `results_full/learning_curves.csv`
- `results_full/rule_change_adaptation.csv`
- `results_full/manifest.json`
- persistente Gewichte je Architektur und Messpunkt
- JSON-Metriken je Checkpoint

Ein vollständig abgeschlossenes Agent/Seed-Paar kann nach einer
Unterbrechung wiederverwendet werden:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.experiment `
  --output results_full --resume
```

Nach dem vollständigen Lauf wählt die vorab festgelegte Regel den
Gewinner, prüft ihn auf 50 zuvor unberührten Partieseeds und schreibt
`winner.json`:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.select_winner `
  --results results_full
```

Anschließend wird genau dieser Checkpoint mit Prüfsumme und
Replikationsmetadaten eingefroren:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.freeze_winner `
  --winner results_full\winner.json `
  --output exports\runenkrieg_frozen_winner.tflite
```

Der byteidentische Import in das getrennte Android-Spiel wird unabhängig
von der Laufzeitprüfung in der App zusätzlich auf dem Build-Rechner
verifiziert:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.verify_android_import `
  --android-model ..\Runenkrieg_TensorFlow_Winner_Android\app\src\main\assets\runenkrieg_frozen_winner.tflite `
  --android-metadata ..\Runenkrieg_TensorFlow_Winner_Android\app\src\main\assets\winner_metadata.json
```

Der präregistrierte Gewinner des abgeschlossenen Fünfseed-Laufs ist der
Contextual Bandit aus Trainingsseed `20260731` am Checkpoint `10.000`.
Seine unabhängige Replikation auf 50 zuvor unberührten Seeds erreichte
60 % Partiensiege. Die paarweisen Unterschiede zu den anderen
Architekturen waren nach Holm-Korrektur nicht statistisch signifikant;
das Ergebnis ist daher eine Auswahl innerhalb dieses Versuchs und keine
allgemeine Überlegenheitsbehauptung.

Die vollständigen Lernkurven werden über die fünf Seeds aggregiert und
mit 95-%-Bootstrapintervallen sowie exakten gepaarten
Sign-Flip-Permutationstests dokumentiert:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.analyze_results `
  --results results_full
```

Das unveränderliche Versuchsdesign steht in
[`EXPERIMENT_PROTOCOL.md`](EXPERIMENT_PROTOCOL.md).

## Interaktives Spiel

Nach einem Trainingslauf kann gegen ein gespeichertes Modell gespielt
werden:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.play `
  --agent mlp `
  --weights results_full\models\seed_20260730\mlp\round_10000\mlp.weights.h5
```

Ein eingefrorenes Modell kann anschließend für die Android-LiteRT-Runtime
exportiert werden:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.export_litert `
  --agent mlp `
  --checkpoint results_full\models\seed_20260730\mlp\round_10000 `
  --output exports\runenkrieg_mlp.tflite
```

## Erhobene Größen

- Holdout-Rundensiegrate,
- mittlere Belohnung,
- Token-Differenz am Partieende,
- mittlere und 95-%-Entscheidungszeit,
- Prozess-CPU-Zeit des Trainings,
- Parameter-Speicher,
- identische Entscheidung nach Speichern und Neuladen,
- Reaktion auf identische Gegenwart mit anderer Vorgeschichte,
- Leistung unmittelbar nach umgekehrter Elementregel,
- Anpassung nach 250, 500 und 1.000 Runden unter der neuen Regel.

Die Android-Messwerte werden ohne erfundene Zwischenwerte in
`tatarus_measurements_template.csv` eingetragen. Erst danach dürfen
Lernkurven oder Sample-Effizienz verglichen werden.

Nach dem Ausfüllen als `tatarus_measurements.csv` entsteht der gemeinsame
Bericht mit:

```powershell
.\.venv\Scripts\python.exe -m runenkrieg_tf.report
```

## Wissenschaftliche Grenze

Ein schneller Lauf ist ein Funktionstest. Ein Nachweis verlangt mehrere
Trainingsseeds, unberührte Holdout-Seeds, Konfidenzintervalle und eine
Replikation. Ein Vorteil in Rundensiegrate allein beweist keine allgemeine
Überlegenheit einer Architektur.

Latenz, CPU-Zeit und Energie dürfen nur direkt verglichen werden, wenn die
Modelle auf demselben Gerät und mit demselben Messverfahren ausgeführt
werden. Desktop-TensorFlow gegen Android-TATARUS wäre hierfür ein
Hardwarekonfund; der LiteRT-Export ist die vorgesehene mobile Messroute.
