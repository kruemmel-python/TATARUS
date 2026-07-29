# Stufe 14: lokale synaptische Eligibility

Diese Stufe implementiert eine eigene signierte Eligibility-Spur für jede
vorhandene rekurrente Verbindung. Sie ist von der cue-gebundenen
Eligibility-Memory des Delayed-XOR-Readouts getrennt.

## Lokale Regel

Für die Verbindung `j → i` gilt:

```text
e(i,j) ← clip(
    e(i,j) × exp(-dt/tau)
    + spike_i × preTrace_j
    - spike_j × postTrace_i,
    -Maximum,
    +Maximum)

m(i,j) = clip(1 + Gain × tanh(e(i,j)), 1-Gain, 1+Gain)
```

Die spätere Übertragung wird mit `m(i,j)` multipliziert. Die Spur wird zu
Beginn jedes Samples zurückgesetzt.

## Entwicklungsablation

Verwendete Parameter:

```text
tau=100 ms
Gain=0.35
Maximum=4
```

Der Lauf verwendet dieselben 16 Seeds wie die bereits ausgewertete
Gedächtnisstufe 13. Er ist deshalb eine gepaarte Entwicklungsablation, keine
neue unabhängige Bestätigung.

Ergebnis:

| Variante | Accuracy | Spikes/korrekt |
|---|---:|---:|
| Vollmodell mit lokaler Spur | 0.891927 | 5.714066 |
| ohne lokale Synapsenspur | 0.892578 | 5.709169 |
| Vorzeichengate mit lokaler Spur | 0.889974 | 5.715085 |

Mit dieser ersten festen Parametrisierung wurde kein Vorteil nachgewiesen.

Der vollständige Lauf wurde in einem zweiten Ausgabeordner wiederholt.
Aggregierter Bericht und Rohdaten waren byte-identisch:

```text
ABLATION_REPORT.md
SHA-256 43B2019354BF4B909D1841F737188FEAD9FF96A97723F72D0296702DF332005D

ablation_raw_results.csv
SHA-256 C3372AD011228DFDDE552BA4E3F3244D3AA6F0968D770973CF92642ED9FC7429
```

## Dateien

- `ABLATION_REPORT.md`: aggregierter Bericht,
- `ablation_raw_results.csv`: 192 Ergebniszeilen plus Kopfzeile,
- `stdout.log` und `stderr.log`: Prozessprotokolle.

## Reproduktion

```powershell
AGBioNetworkDelayedXor.exe <Ausgabeordner> --memory-ablate `
  "50;100;200" 1 1 1 1 "100;0.35;4;40"
```

Der vierte Wert ist die später ergänzte Timingkontrolle; die historische
signierte Stufe-14-Ablation verwendet ihn nicht aktiv.
