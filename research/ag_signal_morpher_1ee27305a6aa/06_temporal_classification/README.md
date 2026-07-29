# Zeitliche Musterklassifikation und native C++-UI

## Forschungsaufgabe

Zwei Klassen aktivieren dieselben beiden Neuronenassemblies, jedoch in
umgekehrter zeitlicher Reihenfolge:

```text
Klasse A: Assembly 0 -> 1 -> 0 -> 1
Klasse B: Assembly 1 -> 0 -> 1 -> 0
```

Rauschen wird pro Sample deterministisch aus Seed, Klasse und Sampleindex
abgeleitet. Als Readout dienen acht Merkmale: die Populationsfeuerraten von
zwei Assemblies in vier Zeitfenstern. Ein L2-regulierter logistischer Readout
wird ausschließlich auf den Trainingsfolds standardisiert und trainiert.

## Oberfläche starten

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\run_ui.bat
```

Direkte ausführbare Datei:

```text
ui_cpp\build\TATARUS_ResearchUI.exe
```

## Bedienung

- **Neuronen:** Populationsgröße von 2 bis 256.
- **Schritte:** Länge eines Samples in Millisekundenschritten.
- **Seed:** bestimmt Topologie, Stimulusrauschen und Zufallsgate.
- **Rauschen σ / Pulshöhe:** Schwierigkeit und Signalstärke.
- **Verbindungsdichte:** Wahrscheinlichkeit rekurrenter Synapsen.
- **Konstantes Gate:** expliziter Kontrollwert; wird auch als Zentrum des
  Zufallsgates verwendet.
- **Samples/Klasse / CV-Folds:** Umfang der stratifizierten Evaluation.
- **Gate-Modus:** Originalkernel, Konstante, deaktiviert, Vorzeichen, `tanh`
  oder Zufall.
- **Gate-Timing:** `RESET_LOCKED` berechnet das wirksame Gate aus dem Zustand
  nach dem Spike-Reset; `EMISSION_STATE` speichert es kausal vor dem Reset.
- **Emissionsfeature:** pre-reset Spannung oder normalisierte rekurrente
  E/I-Balance sowie die Vier-Feature-Projektion. Für die aktuelle
  Forschungsstufe `EMISSION_STATE` und `4-Feature-Projektion` wählen.
- **Projektionsparameter:** `aEI;aV;aO;aISI;sV;sO;tISI`, beispielsweise
  `0.40;0.25;0.15;0.20;1.0;1.0;50.0`. Die ersten vier Werte sind Gewichte,
  danach folgen Steigungs- und Überschussskala sowie die ISI-Zeitkonstante.
- **Biophysik:** Zeitschritt, Membran-/Synapsenzeitkonstante, Potentiale,
  Refraktärzeit, E/I-Anteil, Gewichte und Adaptation sind editierbar.
- **Axon und Synapse:** individuelle Verzögerungen sowie strom- oder
  AMPA-/GABA-leitwertbasierte Übertragung.
- **Dendrit:** optionales passives Zweikompartiment mit einstellbarer
  Zeitkonstante, Kopplung und externem Inputanteil.
- **Operatorökologie:** separate Modi für `EE`, `EI`, `IE`, `II`.
- **Forschungsautomation:** Mehrseed-Vergleich mit gepaartem
  Permutationstest und Holdout-Optimierung der Projektionsgewichte.
- **Timing-Kontrolle:** keine, einen Schritt zeitverschoben oder zwischen
  Neuronen zyklisch state-shuffled.
- **STDP:** lokale Plastizität ein- oder ausschalten.
- **Lokale Synapsen-Eligibility:** eine signierte, exponentiell abklingende
  Kausalitätsspur je Verbindung mit editierbarem `tau`, Gain, Maximum und
  Timing-Shift.

**Einzelsimulation** zeichnet Spike-Raster und Spannung von Neuron 0 und gibt
Aktivität, Feuerrate, globale und event-konditionierte Gate-Statistik,
Gateentropie, Eventfeature-Verteilung, Spannungsenergie,
Population-Spike-Count-Fano, mittlere paarweise Spike-Korrelation und binned
coincidence rate aus.

**Alle Gates vergleichen** berechnet Accuracy, Balanced Accuracy,
Fold-Streuung, Konfusionsmatrix, Feuerrate, mittlere Gatewirkung,
Gateentropie, Assembly-Separation und Spikes je korrekter Entscheidung für
alle sechs Varianten. Die event-gematchte Konstante und das Zufallsgate mit
gematchter wirksamer Verteilung werden dabei automatisch aus einem
Kernel-Kalibrationslauf erzeugt. Die Balkengrafik zeigt die
Cross-Validation-Accuracy.

Für die event-gematchte `RESET_LOCKED`-Kontrolle ist
`0.12831112128784755` voreingestellt. Für `EMISSION_STATE` wurde im
Standardassay `≈0.88934049` gemessen. Für die dynamische E/I-Variante wird der
passende Wert im Vergleich automatisch kalibriert.

**Bericht speichern** schreibt die aktuell angezeigten Parameter und Resultate
als UTF-8-Textdatei.

Die Delayed-XOR-Einzelablationen unterscheiden zwei Speicherarten:

- die cue-gebundene Eligibility im Readout (`50;100;200 ms`),
- die lokale Eligibility jeder rekurrenten Synapse (`100;0.35;4;40`).

Der erste gepaarte 16-Seed-Entwicklungstest der lokalen Spur ergab keinen
Vorteil gegenüber derselben Architektur ohne lokale Spur. Das ist ein
negatives Forschungsergebnis, kein Implementierungsfehler; Details liegen in
`../../14_local_synaptic_eligibility/`.

**Stufe 15: Trace-essential Memory** entfernt die explizite
Readout-Eligibility, erzwingt `400 ms` Nullinput und wertet nur das
post-Recall-Fenster aus. Der eingefrorene signierte Kandidat erreichte auf
zwölf neuen Seeds `0.635417`; ohne Spur und bei `Gain=0` exakt `0.5`. Die
vorab definierte Behauptung wurde nicht bestätigt. Explorativ erreichte die
40-ms-Spur `0.807292`. Details liegen unter
`../../15_trace_essential_memory/`.

## Build und Tests

```powershell
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\build_ui.bat
research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp\build\AGBioNetworkEngineTests.exe
```

Der Build verwendet Visual Studio 2022 Build Tools und erzeugt:

- `TATARUS_ResearchUI.exe` – native Win32-Oberfläche,
- `AGBioNetworkEngineTests.exe` – C++-Invarianztests,
- `AGBioNetworkResearch.exe` – reproduzierbarer C++-Vergleichslauf,
- `AGBioNetworkDelayedXor.exe` – Delayed-XOR-Experimente,
- `AGBioNetworkTraceEssential.exe` – Stufe-15-Suche und Holdout.

## Wissenschaftliche Grenze

Die Oberfläche dient präzisen synthetischen Experimenten. Sie macht aus dem
LIF-Modell kein validiertes Abbild realer Nervensysteme.
