# Forschungsstufe 15: Trace-essential Memory

Status: vor Holdout-Ausführung eingefroren.

Amendment nach dem ersten reinen Entwicklungslauf, vor jedem Zugriff auf die
Holdout-Seeds: Die adaptive Schwelle wurde als alternatives
Gedächtnissubstrat auf `Δθ=0` gesetzt, die reizfreie Verzögerung von `200`
auf `400 ms` verlängert und die zu schwache Ein-Schritt-Timingkontrolle auf
`40 ms` festgelegt. Anschließend wird die gesamte Entwicklung neu gerechnet.

## Hypothese

Eine signierte lokale Eligibility-Spur pro Synapse bewahrt Information über
zwei frühe Cues während einer reizfreien Verzögerung und verändert dadurch
die Reaktion auf einen klassenidentischen Recall-Cue.

## Leakage-freies Aufgabenprotokoll

Simulationsdauer: `560 ms`, `dt=1 ms`.

| Phase | Intervall | Eingang |
|---|---:|---|
| Ruhe | `0...19 ms` | exakt null |
| Cue 1 | `20...39 ms` | Assembly gemäß erstem Bit |
| Lücke | `40...59 ms` | exakt null |
| Cue 2 | `60...79 ms` | Assembly gemäß zweitem Bit |
| reizfreie Verzögerung | `80...479 ms` | exakt null |
| gemeinsamer Recall | `480...499 ms` | identisch auf allen Neuronen |
| Readout | `500...559 ms` | exakt null |

Jeder Cue besitzt unabhängig von seinem Bit dieselbe Dauer, Amplitude,
Neuronenanzahl und Energie. XOR `0` besteht aus `0→0` und `1→1`, XOR `1`
aus `0→1` und `1→0`. Damit sind Zahl und zeitliche Position der Pulse
identisch; nur die Assembly-Identitäten tragen Information.

Recall-Rauschen hängt ausschließlich von Seed und Wiederholung ab und ist
für alle vier Bitkombinationen identisch. Die Engine prüft vor jedem Lauf:

- Verzögerung exakt null,
- Recall und Readout-Eingang zwischen allen Bitkombinationen identisch,
- Gesamtenergie aller vier Eingabemuster identisch.

## Readout

Das lineare logistische Readout erhält ausschließlich aus `500...559 ms`:

- Spikecount jedes Neurons,
- mittlere normalisierte Somamembranspannung jedes Neurons,
- eine innerhalb des Readoutfensters bei null gestartete 20-ms-Spiketrace,
- Populationsspikecounts in drei Subfenstern.

Es erhält keine Cue-Bins, keine Eligibility-Werte, keine cue-gebundene
Readout-Memory und keine Interaktionsprodukte.

## Grobsuche

Nur Entwicklungsseeds:

```text
4001;4051;4099;4153
```

Suchraum:

```text
tau:     20;50;100;200;400 ms
Gain:    0;0.10;0.20;0.35;0.50
Maximum: 0.25;0.5;1;2;4
```

Insgesamt `125` Kombinationen. Pareto-Dominanz wird über maximale Accuracy
und minimale Spikes je korrekter Entscheidung bestimmt. Höchstens fünf
Pareto-Kandidaten erhalten den vollständigen Kontrolllauf.

Zielfunktion:

```text
J = Accuracy
    - 0.0005 × Spikes_pro_korrekte_Entscheidung
    + 0.5 × (Accuracy_Spur - Accuracy_beste_Kontrolle)
```

## Pflichtkontrollen

1. signierte Spur,
2. Spur deaktiviert,
3. `Gain=0`,
4. event-gematchter konstanter Übertragungsfaktor,
5. vorzeichenlose Spur `|e|`,
6. um `40 ms` zeitverschobene Spur,
7. deterministisch zwischen realen Synapsen vertauschte Spur,
8. Vorzeicheninvertierung `e→−e`,
9. zufällige Spurwirkung aus derselben empirischen Faktorverteilung,
10. Spur nur auf `E→E`,
11. Spur nur auf `I→E`.

Konstante und Zufallsverteilung werden pro Netzwerkseed aus den tatsächlich
übertragenen Faktoren des signierten Modells kalibriert, ohne Labels zu
verwenden.

## Gesperrte Holdout-Seeds

```text
4211;4271;4337;4409;4481;4561;
4637;4721;4801;4889;4973;5051
```

Diese Seeds dürfen erst nach Schreiben von `FROZEN_CANDIDATE.txt` simuliert
werden.

## Vorab definierte Entscheidung

`TRACE_ESSENTIAL_MEMORY_CONFIRMED` nur wenn:

- Holdout-Accuracy des signierten Modells mindestens `0.65`,
- Accuracy ohne Spur und bei `Gain=0` jeweils höchstens `0.55`,
- Accuracy-Vorteil gegenüber jeder Kontrolle größer als `0.03`,
- zweiseitiger exakter gepaarter Sign-Flip-Test nach konservativer
  Zehnfach-Holm-Korrektur für jeden Accuracy-Vergleich `<0.05`.

Andernfalls:

```text
TRACE_ESSENTIAL_MEMORY_NOT_CONFIRMED
```

Die synthetische Aufgabe ist keine biologische Validierung.
