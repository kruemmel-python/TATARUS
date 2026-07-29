# Forschungsstufen 20–23 – Reichweite und Replikation

Stand: 29. Juli 2026

Die vier Stufen verwenden eine gemeinsame C++-Pipeline:

`AGStage20To23.exe`

Damit bleiben Nervensystemkern, Seeds, Sicherheitsregeln, Berichte und
Replikationsartefakte konsistent.

## Stufe 20 – Open Lifeworld und G5

Die prozedurale, teilweise beobachtbare Welt enthält frei gezogene
Objektlagen, Energiebedarf, Gefahr, mehrere Ziele, verzögerte Konsequenzen,
unangekündigte Regelwechsel und eine eingefrorene G5-Ereignisstruktur.
Entscheidungen werden vor dem Reward getroffen; der höhere Kern sieht nur
die Cognitive Bridge.

Acht neue Seeds ergaben:

- gekoppeltes Nervensystem: mittlerer Reward `310,157089`,
- gleiche Architektur ohne Eligibility: `294,101531`,
- statischer Reflex: `119,488903`,
- G5: `363,183060`,
- 6/8 Seeds bestanden alle Einzelkriterien.

Status: `confirmed_on_procedural_holdouts`.

## Stufe 21 – Mehrskaliges Gedächtnis

Getrennt geprüft werden:

- episodische Einmalspur gegenüber Ohne-Trace-Kontrolle,
- reward-gebundene Konsolidierung,
- kontrolliertes Vergessen in reizfreier Zeit,
- Reaktivierung eines partiellen alten Cues nach Interferenz.

Eligibility wird nur bei externem Reiz, Recall, Neuheit oder Reward neu
geschrieben. Während echter Leerzeit zerfällt sie, ohne durch spontane
rekurrente Aktivität verdeckt erneuert zu werden.

Über acht Seeds:

- episodisches Signal `0,281266` gegenüber `0`,
- konsolidierte Gewichtsänderung `87,922375`,
- kontrolliertes Vergessen `99,9955 %`,
- Retention nach Interferenz `0,999981`,
- 8/8 Seeds vollständig bestanden.

Status: `confirmed_on_synthetic_holdouts`.

## Stufe 22 – Skalierung

Für Populationen oberhalb 2.048 Neuronen verwendet die Initialisierung einen
direkten Sparse-Sampler mit festem erwarteten Ausgangsgrad. Dadurch entfällt
die quadratische Prüfung aller möglichen Zellpaare.

Gemessen werden:

- Build- und Simulationszeit,
- Echtzeitfaktor,
- Ereignisdurchsatz,
- Synapsenzahl,
- Snapshotgröße sowie Lade-/Speicherzeit,
- exakter Snapshot-Hash,
- Endlichkeit, Energiegrenzen und Dale-Prinzip.

Der normale kostensparende Lauf testet 256, 1.024, 4.096 und 16.384
Neuronen. `--full-scale` ergänzt 65.536 Neuronen.

Der vollständige Release-Lauf hat 65.536 Neuronen mit 2.097.328 aktiven
Synapsen ausgeführt. 40 Simulationsschritte benötigten auf der lokalen
12-Thread-CPU 2.329,165 ms. Der 212.426.348-Byte-Snapshot wurde exakt
restauriert; Endlichkeit, Energiegrenzen und Dale-Prinzip blieben erhalten.

## Stufe 23 – Replikationspaket

Jeder Lauf erzeugt `replication_kit` mit:

- 24 extern verwendbaren neuen Seeds,
- Clean-Build- und Gesamtlaufskript,
- erwarteten Referenzwerten,
- Hardware-/Compilerprotokoll,
- Hashmanifest.

Der lokale Test des Pakets verwendete die tatsächlich erzeugte unabhängige
Seeddatei und bestätigte Stufe 20 sowie Stufe 21 erneut auf 8/8 Seeds. Dies
prüft das Paket, ersetzt aber nicht die geforderte zweite Hardware.

Der lokale Status lautet bewusst nur
`package_ready_local_validation_only`. Eine unabhängige Replikation kann
erst nach Ausführung auf einem zweiten Rechner bestätigt werden.

## Ausführen

```powershell
cd research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp
build_stage20_23.bat build_stage23
build_stage23\AGStage20To23.exe stage20_23_results
```

Vollskalierung:

```powershell
build_stage23\AGStage20To23.exe stage20_23_full --full-scale
```

Maßgeblicher Mehrseed-Bericht:

`../06_temporal_classification/ui_cpp/stage23_final_release_v2`

## Grenzen

Die offene Welt ist prozedural und synthetisch, nicht physisch real.
G5 bestätigt Transfer innerhalb dieser Weltfamilie, keine universelle
Grammatikgeneralisation. Die zweite Hardware-Replikation ist vorbereitet,
aber lokal prinzipbedingt noch nicht ausführbar.
