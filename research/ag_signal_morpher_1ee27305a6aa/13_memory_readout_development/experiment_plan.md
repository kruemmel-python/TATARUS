# Entwicklung eines Delayed-XOR-Gedächtnisreadouts

Festgeschrieben am 28. Juli 2026 vor dem ersten Entwicklungslauf.

## Datentrennung

### Entwicklungs-Seeds

Die 24 Seeds aus der negativen Stufe 12 sind bereits verbraucht und dürfen
für Entwicklung und Auswahl verwendet werden:

```text
1061;1103;1151;1201;1249;1301;1361;1423;
1481;1543;1601;1663;1721;1783;1847;1901;
1973;2039;2111;2179;2251;2333;2411;2503
```

### Gesperrte Bestätigungs-Seeds

Diese Seeds dürfen erst ausgewertet werden, wenn der Readout auf den
Entwicklungs-Seeds mindestens `0.75` Accuracy erreicht und danach ohne
Änderung eingefroren wurde:

```text
2609;2671;2741;2803;2879;2953;3023;3109;
3181;3253;3323;3391;3463;3541;3613;3691
```

## Unveränderte Aufgabe

Stimulus, Bitkombinationen, Rauschen, zwei Gedächtnislücken und Netzwerk
bleiben gegenüber Stufe 12 unverändert. Dadurch kann eine Verbesserung nicht
durch eine leichtere Testaufgabe erklärt werden.

## Gedächtniszustände

Der neue Readout verwendet ausschließlich Zustände ab Ende des zweiten
Hinweises. Frühere Rohbins werden nicht direkt übergeben.

Für jede Assembly werden an drei Zeitpunkten gespeichert:

- exponentiell gefilterte Spikeaktivität mit `tau = 20, 50, 100, 200 ms`,
- Spikecounts der letzten `10 ms` und `30 ms`,
- mittlere Somamembranspannung,
- mittlere dendritische Spannung.

Das ergibt zunächst 48 kausale Zustandsmerkmale. Ergänzt werden:

- paarweise Produkte der acht Assembly-Differenzkanäle innerhalb jedes
  Abtastzeitpunkts,
- zeitübergreifende Produkte desselben Differenzkanals.

Alle Interaktionen stammen aus Netzwerkzuständen. Bits oder Zielklasse werden
nicht direkt als Readoutmerkmale übergeben.

## Readout

- logistische Regression,
- Standardisierung nur auf dem jeweiligen Trainingsfold,
- Lernrate `0.12`,
- `300` Epochen,
- L2 `0.003`,
- vier Folds anhand des Wiederholungsindex.

## Entwicklungsentscheidung

Der Readout wird eingefroren, wenn auf den 24 Entwicklungs-Seeds:

1. die über beide Verzögerungen gemittelte Kernel-Accuracy mindestens
   `0.75` erreicht,
2. jede Verzögerungsbedingung mindestens `0.65` erreicht.

Sind diese Kriterien nicht erfüllt, bleiben die Bestätigungs-Seeds gesperrt.
Weitere Änderungen müssen ausschließlich anhand der Entwicklungs-Seeds
erfolgen und im Entwicklungsprotokoll stehen.

## Bestätigungsentscheidung

Nach dem Einfrieren gilt Delayed XOR als zuverlässig gelernt, wenn:

1. mittlere Kernel-Accuracy auf den 16 neuen Seeds mindestens `0.75`,
2. untere einseitige 95-%-Bootstrapgrenze mindestens `0.65`,
3. beide Verzögerungsbedingungen einzeln mindestens `0.65`.

Erst danach werden Spikekostenvergleiche gegen Konstante, deaktiviert,
Vorzeichen, Tanh und Zufall interpretiert.
