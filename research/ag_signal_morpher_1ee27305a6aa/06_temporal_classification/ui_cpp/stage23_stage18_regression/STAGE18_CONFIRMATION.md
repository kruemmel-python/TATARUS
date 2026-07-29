# Forschungsstufe 18 – unabhängige Endzielbestätigung

Status: `confirmed`

Die Kriterien wurden vor dem Lauf eingefroren. Acht neue Netzwerkseeds wurden weder für Mechanismuswahl noch Parameteroptimierung verwendet.

Die Reparaturprüfung fordert einen Baseline-Effekt >=0,03, einen messbaren Verlust >0,004, gleiches Funktionsvorzeichen, mindestens 70 % Wiedergewinn und mindestens eine neue Synapse mit Eltern-Provenienz zum zerstörten Pfad.

| Hypothese | bestandene Seeds | Kriterium | Status |
|---|---:|---:|---|
| stabile konkurrierende Repräsentationen | 8/8 | >=6 | bestätigt |
| tokenizerfreie Übergänge und Grenzen | 8/8 | >=6 | bestätigt |
| Trace-essential Recall-XOR | 12 Holdout-Netze | Accuracy>=0.70, Kontrolle<=0.60, Vorteil>=0.15 | bestätigt |
| Funktionsreparatur mit Pfadprovenienz | 8/8 | >=6 | bestätigt |

## Aggregierte Werte

- Assemblies pro Netz: 5.750000
- Reaktivierung ähnlicher Reize: 0.902569
- Repräsentation nach Schaden: 0.853641
- Übergangsaccuracy: 0.773438
- Grenzreaktionsfaktor: 2.883236
- Trace-Accuracy / Kontrolle: 1.000000 / 0.486111
- mittlerer wiedergewonnener Funktionsanteil: 1.113726
- mittlere Ersatzsynapsen mit Eltern-Provenienz: 6.000000

Die Bestätigung gilt für die definierten synthetischen Aufgaben und ist keine Behauptung biologischer Gleichwertigkeit oder allgemeiner Intelligenz.
