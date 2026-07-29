# Vorab festgelegter Mehrseed-Überlegenheitstest

Festgeschrieben am 28. Juli 2026 vor Ausführung des in diesem Ordner
implementierten Bestätigungslaufs.

## Wissenschaftliche Grenze

Untersucht wird ausschließlich das synthetische GO-SNN und seine zeitliche
Reihenfolgeaufgabe. Ein positives Resultat wäre keine biologische
Validierung und kein Beweis allgemeiner Überlegenheit.

Die bereits ausgewerteten Pilot-Seeds `11`, `23`, `38`, `53`, `71` werden
nicht als Bestätigungsdaten verwendet.

## Bestätigungs-Seeds

```text
89;107;131;149;173;197;223;251;281;313;347;383;
421;463;509;557;607;659;719;773;829;887;947;1013
```

Die Seeds werden vollständig ausgewertet. Nachträgliches Entfernen,
Ersetzen oder Sortieren anhand der Ergebnisse ist unzulässig.

## Gemeinsame Architektur

- ereigniskausales Timing `EMISSION_STATE`,
- Vier-Feature-Projektion `0.40;0.25;0.15;0.20`,
- AMPA-/GABA-Leitwertsynapsen,
- individuelle Axonverzögerungen von `1` bis `5 ms`,
- passives Dendritenkompartiment,
- lokale STDP aktiviert,
- Basisstrom `15.0`,
- `16` Samples je Klasse,
- `4` stratifizierte Folds,
- `200` Readout-Epochen.

`compareAll()` setzt für jede Kontrolle alle vier Verbindungsklassen
homogen auf denselben Modus. Konstante und Zufallsverteilung werden anhand
der tatsächlich wirksamen Kernelereignisse kalibriert.

## Belastungsbedingungen

| Kennung | Neuronen | Dichte | Schritte | Puls | Rauschen σ | Zweck |
|---|---:|---:|---:|---:|---:|---|
| `standard` | 16 | 0.18 | 120 | 2.0 | 5.0 | erweiterte Referenz |
| `high_noise` | 16 | 0.18 | 120 | 2.0 | 8.0 | Rauschrobustheit |
| `weak_signal` | 16 | 0.18 | 120 | 1.0 | 5.0 | schwaches Eingangssignal |
| `sparse_network` | 24 | 0.10 | 120 | 2.0 | 5.0 | Topologierobustheit |

Statistische Einheit ist der Netzwerk-Seed. Zuerst werden die vier
Bedingungen innerhalb jedes Seeds arithmetisch gemittelt; die Tests verwenden
anschließend genau 24 gepaarte Seedwerte.

## Kontrollen

```text
Originalkernel
event-gematchte Konstante
deaktiviert
Vorzeichengate
Tanh
verteilungsgemachtes deterministisches Zufallsgate
```

## Vorab definierte Behauptungen

### A. Formelgeometrische Accuracy-Überlegenheit

Diese starke Behauptung ist nur zulässig, wenn alle Bedingungen erfüllt sind:

1. mittlere gepaarte Accuracy-Differenz `Kernel − Kontrolle > 0`,
2. einseitiger gepaarter Sign-Flip-Permutationstest nach Holm-Korrektur
   `p < 0.05`,
3. Bedingung 1 und 2 gelten sowohl gegen die event-gematchte Konstante als
   auch gegen das verteilungsgematchte Zufallsgate.

### B. Systemische Effizienzüberlegenheit

Diese engere Behauptung ist nur zulässig, wenn alle Bedingungen erfüllt sind:

1. untere einseitige 95-%-Bootstrapgrenze der Accuracy-Differenz liegt über
   der Nichtunterlegenheitsmarge `−0.02`,
2. `Spikes je korrekter Entscheidung` sind niedriger,
3. der einseitige gepaarte Sign-Flip-Test der Kostendifferenz ist nach
   Holm-Korrektur `p < 0.05`,
4. Bedingungen 1 bis 3 gelten gegen event-gematchte Konstante und
   deaktiviertes Gate.

Eine Effizienzbehauptung gegen das Zufallsgate wird separat berichtet und ist
für die Behauptung eines formelspezifischen Vorteils erforderlich.

## Statistik

- deterministischer Monte-Carlo-Sign-Flip-Test mit `1.000.000`
  Permutationen je Vergleich,
- einseitige Tests in der vorab festgelegten Vorteilsrichtung,
- Holm-Korrektur getrennt für Accuracy- und Effizienzhypothesen über alle
  fünf Kontrollen,
- deterministischer gepaarter Bootstrap mit `200.000` Stichproben,
- Nichtunterlegenheitsmarge der Accuracy: `−0.02`,
- keine nachträgliche Aufgaben-, Seed- oder Metrikauswahl.

## Entscheidung

Wenn die Kriterien nicht erfüllt werden, lautet das Ergebnis ausdrücklich
`NO_SUPERIORITY_DEMONSTRATED`. Ein technisch stabiler Lauf, eine einzelne
bessere Bedingung oder ein unkorrektierter p-Wert genügt nicht.
