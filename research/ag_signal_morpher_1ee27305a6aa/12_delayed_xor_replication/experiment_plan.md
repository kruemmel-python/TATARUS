# Vorab festgelegte Replikation auf Delayed XOR

Festgeschrieben am 28. Juli 2026 vor dem ersten Delayed-XOR-Datenlauf.

## Ziel

Die in `11_superiority_multiseed` beobachtete Spikekosteneffizienz wird auf
einer neuen Aufgabenfamilie geprüft. Delayed XOR enthält zwei zeitlich
getrennte binäre Hinweise. Die Zielklasse ist:

```text
y = bit_1 XOR bit_2
```

Ein einzelner Hinweis reicht nicht für die Entscheidung. Das lineare Readout
muss eine nichtlineare, zeitabhängige Reservoirrepräsentation nutzen.

## Neue Bestätigungs-Seeds

Keiner der folgenden Seeds wurde in Stufe 10 oder 11 verwendet:

```text
1061;1103;1151;1201;1249;1301;1361;1423;
1481;1543;1601;1663;1721;1783;1847;1901;
1973;2039;2111;2179;2251;2333;2411;2503
```

Alle 24 Seeds werden vollständig ausgewertet. Nachträgliches Entfernen oder
Ersetzen ist unzulässig.

## Delayed-XOR-Bedingungen

| Bedingung | Schritte | erster Hinweis | zweiter Hinweis | Zweck |
|---|---:|---:|---:|---|
| `medium_delay` | 160 | 20–39 | 80–99 | mittlere Gedächtnislücke |
| `long_delay` | 160 | 10–29 | 110–129 | lange Gedächtnislücke |

`bit=0` stimuliert Assembly 0, `bit=1` Assembly 1. Jede der vier
Bitkombinationen besitzt zwölf verrauschte Wiederholungen. Damit entstehen
48 Samples je Modus und Bedingung. Vier stratifizierte Folds verwenden den
Wiederholungsindex modulo vier.

## Netzwerk und Readout

- 24 Neuronen, exzitatorischer Anteil `0.8`,
- Verbindungsdichte `0.15`,
- AMPA-/GABA-Leitwertsynapsen,
- individuelle Axonverzögerungen `1...5 ms`,
- passives Dendritenkompartiment,
- ereigniskausale Vier-Feature-Projektion,
- STDP aktiviert,
- Basisstrom `15.0`, Pulshöhe `2.5`, Rauschen `5.0`,
- acht Zeitfenster,
- pro Zeitfenster und Assembly: Feuerrate und mittlere Membranspannung,
- 32 Readoutmerkmale,
- logistische Regression mit Lernrate `0.12`, 300 Epochen, L2 `0.003`.

Standardisierung und Training verwenden ausschließlich den jeweiligen
Trainingsfold.

## Kontrollen

```text
Originalkernel
event-gematchte Konstante
deaktiviert
Vorzeichengate
Tanh
verteilungsgematchtes deterministisches Zufallsgate
```

Das Vorzeichengate ist die vorab benannte stärkste Sparsamkeitsbaseline.

## Statistische Einheit

Die beiden Verzögerungsbedingungen werden zuerst innerhalb jedes Seeds
gemittelt. Die statistischen Tests verwenden genau 24 gepaarte Seedwerte.

## Replikationskriterien

### Scoped-Effizienzreplikation

Die frühere eng begrenzte Behauptung gilt als repliziert, wenn für
event-gematchte Konstante, deaktiviertes Gate und
verteilungsgematchtes Zufallsgate jeweils gilt:

1. untere einseitige 95-%-Bootstrapgrenze von
   `Accuracy_Kernel − Accuracy_Kontrolle > −0.03`,
2. `Spikes/korrekte Entscheidung_Kontrolle − Kernel > 0`,
3. einseitiger gepaarter Sign-Flip-Test der Kostendifferenz nach
   Holm-Korrektur über alle fünf Kontrollen `p < 0.05`.

### Starke Replikation gegen alle Kontrollen

Zusätzlich müssen dieselben Kriterien gegen Vorzeichen und Tanh erfüllt sein.
Nur dann darf Überlegenheit gegenüber der stärksten Sparsamkeitsbaseline
behauptet werden.

### Accuracy-Überlegenheit

Wird separat geprüft und verlangt eine positive Accuracy-Differenz sowie
Holm-korrigiertes `p < 0.05`. Sie ist nicht Voraussetzung der
Effizienzreplikation.

## Statistik

- `1.000.000` deterministische Monte-Carlo-Sign-Flip-Permutationen je Test,
- `200.000` deterministische gepaarte Bootstrapstichproben,
- Holm-Korrektur getrennt für Accuracy und Kosten,
- keine nachträgliche Auswahl von Seeds, Verzögerung, Featuretyp oder
  Kontrollgruppe.

## Entscheidung

```text
SCOPED_EFFICIENCY_REPLICATED
STRONG_ALL_CONTROL_EFFICIENCY_REPLICATED
NO_EFFICIENCY_REPLICATION
```

Die Resultate bleiben Aussagen über ein synthetisches Forschungsmodell.
