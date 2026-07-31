# RUNENKRIEG-TATARUS-MULTISEED-1

Dieses Protokoll schließt die asymmetrische Behandlung von TensorFlow und
TATARUS. Es wurde vor dem vollständigen TATARUS-Lauf festgeschrieben.

## Training

- Architektur: TATARUS LargeScale
- Neuronen: 1.024
- Synapsen: 32.768
- Eingabekanäle: 128
- unabhängige Modell- und Umweltseeds:
  `20260730`, `20260731`, `20260732`, `20260733`, `20260734`
- Checkpoints: 250, 500, 1.000, 2.000, 5.000 und 10.000 Runden
- Trainingsgegner: dieselbe gemischte Strategie wie im konventionellen
  Benchmark

Jeder Seed beginnt mit einer neuen neuronalen Topologie, leeren
Readouts, leeren Eligibility-Spuren und null Beobachtungen.

Jedes Checkpointsegment besitzt einen deterministischen Teilstrom mit
`seed xor 0x0A11CE xor vorheriger_Checkpoint`. Dadurch kann ein
mehrstündiger Gerätelauf exakt am letzten vollständig geschriebenen
Checkpoint fortgesetzt werden, ohne einen nicht serialisierbaren
Zufallszustand erraten zu müssen.

Der Orchestrator ist zusätzlich an Geräteseriennummer und Modellkennung
gebunden. Ein USB-Wechsel auf ein anderes Smartphone stoppt den Lauf,
bevor weitere Werte geschrieben werden. Bei einer Wiederherstellung
werden Lernkurve und letzter vollständiger Snapshot zuerst zurück auf das
registrierte Gerät übertragen.

## Lernfreie Holdouts

Jeder Checkpoint wird auf den 20 Seeds `30000` bis `30019` ausgewertet.
Dabei gelten:

- Modus `FROZEN_TATARUS`,
- keine Exploration,
- keine Readoutänderung,
- keine synaptische Plastizität,
- keine Eligibility- oder Assembly-Schreiboperation,
- vollständige Wiederherstellung des Trainingssnapshots nach dem
  Holdout.

## Vorregistrierte Auswahl

Der mobile TATARUS-Gewinner wird bei 10.000 Runden gewählt nach:

1. höchste Partiensiegrate auf dem Holdout,
2. höchste mittlere Token-Differenz,
3. niedrigste mittlere Entscheidungszeit,
4. niedrigster Seed.

Erst danach wird genau ein Snapshot auf den unberührten Seeds
`60000` bis `60049` repliziert und in die getrennte Android-App
übernommen.

## Vergleichsgrenze

Die TensorFlow-Modelle wurden in einer Python-Paritätsumgebung trainiert,
TATARUS läuft nativ im Kotlin-Spielkern. Gleiche Seeds bezeichnen daher
dieselben registrierten Seednummern, aber wegen unterschiedlicher
Zufallszahlengeneratoren nicht zwingend identische Kartenfolgen.

Eine streng paarweise Architekturaussage verlangt zusätzlich
Cross-Language-Goldentests oder einen gemeinsamen vorab generierten
Episodenstrom. Bis dahin sind die Lernkurven symmetrisch geplant, aber
nicht als bitidentische Umweltexposition zu bezeichnen.
