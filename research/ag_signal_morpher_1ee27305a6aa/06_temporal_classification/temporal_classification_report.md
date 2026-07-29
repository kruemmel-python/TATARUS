# Bericht der nächsten Forschungsstufe

## Fragestellung

Kann das Kernel-modulierte Spiking-Netz die zeitliche Reihenfolge zweier
Assemblies aus verrauschten Sequenzen decodierbar machen, und ist diese Wirkung
stärker als geeignete Kontrollen?

## Protokoll

- fünf unabhängige Netzwerk-/Datenseeds,
- je 16 Samples pro Klasse und Seed,
- vier stratifizierte Folds,
- 16 LIF-Neuronen, 120 Schritte, vier Zeitfenster,
- Pulshöhe `2,0`, Rauschstandardabweichung `5,0`,
- lokale STDP in allen Varianten,
- acht aggregierte Spike-Features,
- Standardisierung und logistisches Training nur innerhalb der Trainingsfolds.

Das konstante Gate wurde in der korrigierten Auswertung pro Seed auf den
event-konditionierten Mittelwert der tatsächlich übertragenen Spikes gesetzt.
Für `RESET_LOCKED` ist dies exakt `0,12831112128784755`.

## Mehrseed-Ergebnis

| Gate | Accuracy | Seed-Streuung | mittlere Feuerrate |
|---|---:|---:|---:|
| Originalkernel | 95,625 % | 2,500 % | 2,826 Hz |
| event-gematchte Konstante | 95,625 % | 2,500 % | 2,826 Hz |
| deaktiviert | 90,000 % | 3,062 % | 6,022 Hz |
| Vorzeichengate | **96,875 %** | 2,795 % | 2,764 Hz |
| `tanh`-Gate | 95,625 % | 3,187 % | 2,689 Hz |
| Zufallsgate um event-gematchtes Mittel | 95,625 % | 4,677 % | 2,956 Hz |

## Interpretation

- **Experimentell bestätigt:** Der Kernel und die event-gematchte Konstante
  liefern identische Accuracy und Feuerrate.
- **Experimentell bestätigt:** Der frühere Vorteil gegenüber der global
  gematchten Konstante war ein Kontrollfehler durch wirkungslose Gatewerte.
- **Experimentell bestätigt:** Ein einfaches Vorzeichengate ist im Mittel
  besser; das `tanh`-Gate erreicht denselben Mittelwert wie der Kernel.
- **Ableitung:** Der Nutzen hängt wesentlich an Polaritätsseparation und
  Aktivitätsdämpfung, nicht nachweislich an einer einzigartigen Feinstruktur
  der generierten Formel.
- **Nicht bestätigt:** allgemeine Überlegenheit, biologische Gültigkeit oder
  Neuheit.

Die Einzelresultate stehen in `classification_results.csv`; Aggregation und
Kontrollbeschreibung in `classification_results.json`.

## C++-Referenzlauf

Die native C++-Engine wurde mit MSVC gebaut und getestet. Die UI zeigt jetzt
Gate-Timing, globale und event-konditionierte Gatewerte sowie die korrigierten
Kontrollen. Ein Einzelseed-Lauf ersetzt nicht die obige Mehrseed-Auswertung.

## Nächste Falsifikation

Die sinnvolle nächste Entscheidung ist kein weiterer synthetischer
Parametervergleich, sondern:

1. eingefrorene Hyperparameter,
2. unabhängiger Holdout-Seedbereich,
3. reale oder realistisch simulierte Ereignisdaten,
4. Vergleich der Kalibrierung und Energieeffizienz zusätzlich zur Accuracy,
5. statistischer gepaarter Test Kernel gegen Vorzeichen- und `tanh`-Gate.

Entscheidung: `KEEP_AND_RESEARCH`
