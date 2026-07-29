# Forschungsstufe 15: Trace-essential Memory

Diese Stufe entfernt die explizite cue-gebundene Readout-Memory aus der
Aufgabe. Zwei frühe Cues werden von `400 ms` exakt reizfreier Verzögerung,
einem für alle Klassen identischen Recall-Cue und einem ausschließlich
post-Recall arbeitenden linearen Readout gefolgt.

## Entwicklung

Die erste Pilotfassung zeigte, dass die adaptive Schwelle selbst
Cue-Information tragen konnte. Vor Zugriff auf Holdout-Seeds wurde deshalb:

- `Δθ=0` gesetzt,
- die reizfreie Phase auf `400 ms` verlängert,
- die Timingkontrolle von `1 ms` auf `40 ms` verschärft.

Danach wurden alle 125 Kombinationen erneut auf vier Entwicklungsseeds
berechnet. Eingefrorener Kandidat:

```text
tau=400 ms
Gain=0.50
Maximum=1
```

## Eingefrorener Holdout

Experimenthash:

```text
688F65FA0F77947C
```

Zwölf vorher unbenutzte Netzwerkseeds:

```text
4211;4271;4337;4409;4481;4561;
4637;4721;4801;4889;4973;5051
```

Zentrale Ergebnisse:

| Variante | Accuracy | Spikes/korrekt |
|---|---:|---:|
| signierte Spur | 0.635417 | 91.534899 |
| ohne Spur | 0.500000 | 122.312500 |
| Gain=0 | 0.500000 | 122.312500 |
| event-gematchte Konstante | 0.500000 | 114.666667 |
| 40-ms-verschobene Spur | 0.807292 | 77.072563 |
| synapsenvertauschte Spur | 0.585938 | 108.192828 |
| verteilungsgematchter Zufall | 0.541667 | 109.103863 |
| nur E→E | 0.622396 | 93.380148 |
| nur I→E | 0.518229 | 120.623914 |

Die vorab definierte strikte Entscheidung lautet:

```text
TRACE_ESSENTIAL_MEMORY_NOT_CONFIRMED
```

Die signierte Spur verfehlt die Accuracy-Grenze von `0.65` knapp und ist
nicht besser als jede Timingkontrolle.

Explorativ ist der mechanistische Befund dennoch stark: Ohne Spur, bei
`Gain=0` und mit event-gematchter Konstante fällt das System exakt auf
Zufallsniveau. Die lokale 40-ms-Spur erreicht `0.807292`; ihr gepaarter
Vorteil gegenüber ohne Spur beträgt `0.307292`, unkorrigiertes exaktes
`p=0.000488`. Das spricht dafür, dass die lokale Spur-Familie ein
Gedächtnissubstrat sein kann, bestätigt aber noch nicht das ursprünglich
eingefrorene signierte Timing.

Der Holdout wurde unverändert wiederholt. Die Rohdaten waren byte-identisch:

```text
SHA-256
DAF07CBC93F6D220679043C4093CC2C2964D015038AF84A252C0CF7FFDEC1DFF
```

## Dateien

- `experiment_plan.md`: vorab definierter Plan und Amendment vor Holdout,
- `development/`: kanonische 125er Suche und eingefrorener Kandidat,
- `development_pilot_pre_amendment/`: Pilot vor Härtung der Nullphase,
- `holdout/REPORT.md`: kanonischer Ergebnisbericht,
- `holdout/holdout_raw.csv`: 132 gepaarte Seed-/Kontrollzeilen,
- `holdout_repeat/`: unveränderte Reproduktion.

## Ausführung

```powershell
AGBioNetworkTraceEssential.exe <Ordner> --develop
AGBioNetworkTraceEssential.exe <Ordner> --confirm "400;0.5;1"
AGBioNetworkTraceEssential.exe <Ordner> --full
```
