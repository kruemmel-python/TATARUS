Artifact: ag_signal_morpher_1ee27305a6aa  
Original source domain: signal_transform  
Assigned wrapper: signal_morpher  
Observed mathematical class: begrenzter Polaritätsseparator mit scharfem Nullübergang und zwei Plateaus  
Strongest measured role: reset-gekoppelter Post-Spike-Übertragungsdämpfer  
Most surprising plausible role: zeitpositionsabhängiger Refraktär-/Kopplungsoperator  
Project integration status: integrated_experimental, event_causal_dynamic  
Evidence level: E/I-getriebenes dynamisches Event-Gate über fünf Seeds bestätigt; kein Vorteil gegenüber der Event-Konstante

# 1. Technische Integrität

Alle 14 Exportartefakte wurden inventarisiert und per SHA-256 erfasst. Die
Originaldateien unter `exports/` blieben unverändert. Python-Referenz und
Originalexport stimmen auf 2.052 Eingaben exakt überein. Fünfzehn Python-Tests
und die nativen MSVC-C++-Tests bestehen. OpenCL wurde nicht ausgeführt.

# 2. Implementierungsgetreue Mathematik

`sdiv(x,x)` berechnet `x/(|x|+10^-6)`, nicht `1`. Der identische
Subtraktionsterm im Nenner ist exakt null, wodurch der geschützte Quotient den
`tanh`-Term fast überall in eines von zwei Plateaus treibt. Der Kernel ist
global zustandsabhängig, komprimiert aber große Eingabebereiche nahezu auf
Polarität.

# 3. Entscheidend wirksame Operatorwerte

Die ursprüngliche Übertragung lautete:

```text
I_syn,i(t) += w_ij * previous_spike_j(t-1) * gate_j(current_voltage(t))
```

Ein Neuron mit `previous_spike=1` wurde bereits auf `-70 mV` zurückgesetzt.
Bei Ruhepotential `-65 mV` und Schwelle `-50 mV` gilt:

```text
z_reset = -1/3
g_reset = 0.1283111212878475
```

Über fünf Seeds wurden ausschließlich für tatsächlich übertragene Spikes
gemessen:

```text
effective_gate_mean     = 0.1283111212878475
effective_gate_variance = 0
minimum = maximum       = 0.1283111212878475
```

Die event-gematchte Konstante reproduziert alle Spannungen und Spikes exakt:
RMSE `0`, Spikeabweichung `0`. Damit ist die frühere Behauptung eines
fortlaufend zustandsabhängigen synaptischen Gates im aktuellen Wrapper
widerlegt.

# 4. Korrigierte Operatorökologie

- **RESET_LOCKED:** experimentell bestätigter konstanter Post-Spike-Dämpfer;
  jeder rekurrente Spike wird auf ungefähr 12,83 % abgeschwächt.
- **EMISSION_STATE:** Gate wird kausal vor dem Reset gespeichert. Das wirksame
  Mittel liegt bei `0,8893404879`, die maximale Varianz aber nur bei
  `7,4×10^-17`. Pre-reset Spannung liegt ebenfalls auf einem Plateau.
- **Globale Konstante:** die frühere Kontrolle `≈0,812` matchte überwiegend
  Gatewerte, die nie mit einem Spike multipliziert wurden.
- **Zeitverschoben/state-shuffled:** machen die Bedeutung von Ausführungszeit
  und neuronaler Zuordnung messbar.
- **Exakter Resetoperator:** bleibt als direkte Kernelrolle ohne Wirkung.

# 5. Metrikkorrektur

Der bisherige „Synchronieindex“

```text
Var(Populationsspikezahl) / (Mean(Populationsspikezahl)+epsilon)
```

ist ein Fano-ähnlicher Burstigkeitsindex und heißt nun
`population_spike_count_fano`. Zusätzlich werden
`mean_pairwise_spike_correlation` und `binned_coincidence_rate` ausgegeben.
Damit werden zeitliche Bündelung und echte paarweise Koinzidenz getrennt.

# 6. Ursprüngliche Netzwerkablation, korrekt interpretiert

| Variante | Feuerrate | Spikes | Spannungsenergie | Population-Count-Fano |
|---|---:|---:|---:|---:|
| RESET_LOCKED-Kernel | 16,687 Hz | 168,2 | 0,6158 | 4,8114 |
| globale Konstante ≈0,812 | 25,139 Hz | 253,4 | 0,5079 | 1,4617 |
| deaktiviert | 27,143 Hz | 273,6 | 0,4940 | 1,4590 |

Diese Unterschiede sind real, belegen aber nur die starke konstante
Reset-Dämpfung gegenüber schwächeren Dämpfungen. Gegen die korrekte
event-gematchte Konstante verschwindet der Unterschied vollständig.

# 7. Zeitliche Musterklassifikation

Die korrigierte Mehrseed-Cross-Validation verwendet dieselben zeitlichen
Assemblies, vier Folds und acht Spikefeatures:

| Gate | Accuracy |
|---|---:|
| RESET_LOCKED-Kernel | 95,625 % |
| event-gematchte Konstante | 95,625 % |
| deaktiviert | 90,000 % |
| Vorzeichengate | 96,875 % |
| `tanh`-Gate | 95,625 % |
| Zufallsgate um event-gematchtes Mittel | 95,625 % |

Der Kernel übertrifft die wirksame Konstantkontrolle nicht. Das einfache
Vorzeichengate ist im Mittel leicht besser. Eine einzigartige Formelwirkung
ist für diese Aufgabe nicht bestätigt.

# 8. Integration und Oberfläche

Die native C++/Win32-Oberfläche unterstützt:

- `RESET_LOCKED` und `EMISSION_STATE`,
- pre-reset Spannung und normalisierte E/I-Balance als Emissionsfeatures,
- eine kausale Vier-Feature-Projektion aus E/I-Balance, Membransteigung,
  Schwellenüberschuss und Inter-Spike-Intervall,
- präzise UI-Eingabe aller vier Gewichte und drei Feature-Skalen,
- explizite Spikeereignisse mit Quelle, Zeitpunkt, Amplitude, Gate und Feature,
- Original-, Konstant-, deaktiviertes, Vorzeichen-, `tanh`- und Zufallsgate,
- zeitverschobene und state-shuffled Kontrollen,
- automatisch event-gematchte Konstant- und Verteilungskontrollen im Vergleich,
- Gatevarianz, Gateentropie und Eventfeature-Statistik,
- Spike-Raster und Spannungsverlauf,
- Fano-, paarweise Korrelations- und Koinzidenzmetriken,
- Cross-Validation, Assembly-Separation, Spikes je korrekter Entscheidung und
  Berichtsexport.

`GateMode.DISABLED` bleibt der vollständige Rückfallpfad. Die voreingestellte
Konstantkontrolle für `RESET_LOCKED` ist `0,12831112128784755`.

# 9. Wissenschaftliche Klassifikation

- **Fakt:** Der Kernel selbst ist global eingabeabhängig.
- **Fakt:** Im ursprünglichen Wrapper ist das wirksame Gate konstant und
  resetgebunden.
- **Experimentell bestätigt:** Event-Konstante und Kernel sind bitgenau
  äquivalent.
- **Experimentell bestätigt:** `EMISSION_STATE` korrigiert die Kausalität, ist
  mit pre-reset Spannung aber weiterhin praktisch konstant.
- **Experimentell bestätigt:** Die normalisierte E/I-Balance erzeugt eine
  wirksame Gatevarianz von `0,06980` und eine Gateentropie von `0,8546 bit`.
- **Experimentell bestätigt:** In der Reihenfolgeerkennung erreicht die
  Event-Konstante `95,00 %`, der E/I-Kernel `90,83 %`.
- **Nicht bestätigt:** biologische Gültigkeit, Neuheit oder allgemeine
  Überlegenheit.

# 10. Pflichtfragen in Kurzform

1. Der Code berechnet einen geschützten Polaritätsseparator.
2. `sdiv` und `sanitize` weichen von gewöhnlicher Algebra ab.
3. Trigonometrische/logarithmische Reststruktur stammt aus der Formel.
4. Die extreme Stufe stammt aus der Schutzdivision.
5. Informativ ist vor allem die enge Nullumgebung.
6. Es existieren zwei Plateaus und zwei Iterationsattraktoren.
7. `tanh(K)` konvergiert abhängig von der Startpolarität.
8. Additiv verschiebt der Kernel synthetische Attraktoren.
9. Multiplikativ ist er im aktuellen Wrapper ein konstanter Reset-Dämpfer.
10. Als Kopplungsparameter hängt seine Rolle entscheidend vom Timing ab.
11. Kandidaten sind Neurodynamik, Regelung und Ereignisverarbeitung.
12. Am stärksten belegt ist reset-gekoppelte Spikeabschwächung.
13. Am überraschendsten ist die vollständige event-konditionierte Konstanz.
14. Event-Konstante, Verteilung, Zeitverschiebung und Shuffle falsifizieren Dynamik.
15. Der Kernel übertrifft die korrekte Event-Ablation nicht.
16. Der Phänotyp ist wrapper- und timingbedingt.
17. Sicherer Integrationspunkt ist ein explizites Gate-Timing-Modul.
18. Python/C++-Modelle, UI, Metriken, Tests und Berichte wurden korrigiert.
19. `GateMode.DISABLED` setzt den neutralen Faktor `1`.
20. Fakten, Ableitungen, Hypothesen und negative Befunde sind getrennt.

# 11. Forschungsstufe 2: E/I-Event-Gate

Die normalisierte E/I-Balance wird bei jeder Schwellenüberschreitung vor dem
Reset erfasst. Der Gatewert wird kausal im `SpikeEvent` gespeichert. Über fünf
Seeds ist das wirksame Gate nun eindeutig dynamisch:

```text
effective_gate_variance = 0.0697995
effective_gate_entropy  = 0.854631 bit
```

| Variante | Reihenfolge-Accuracy | Assembly-Separation | Spikes/korrekte Entscheidung |
|---|---:|---:|---:|
| E/I-Originalkernel | 90,83 % | 2,6417 | 11,6909 |
| event-gematchte Konstante | 95,00 % | 2,5865 | 11,3795 |
| verteilungsgematchtes Zufallsgate | 91,67 % | 2,6489 | 11,6290 |
| deaktiviert | 88,33 % | 2,4274 | 13,9091 |

Damit ist echte Ereignisdynamik bestätigt, aber kein spezifischer Vorteil der
Kernelgeometrie. Der vollständige Bericht liegt unter
`08_event_causal_ei/event_causal_ei_report.md`.

# 12. Forschungsstufe 3: Vier-Feature-Projektion

Die implementierte Projektion lautet:

```text
phi = 0.40*E/I + 0.25*Membransteigung
    + 0.15*Schwellenüberschuss + 0.20*ISI-Zustand
```

| Variante | Accuracy | Assembly-Separation | Spikes/korrekte Entscheidung |
|---|---:|---:|---:|
| Vier-Feature-Kernel | 90,83 % | 3,1124 | 7,9600 |
| Event-Konstante | 88,33 % | 2,9578 | 10,9533 |
| Verteilungsgematchtes Zufallsgate | 90,83 % | 2,7051 | 10,6055 |
| Vorzeichenprojektion | 91,67 % | 3,0634 | 7,7697 |
| ISI-Kernel | 95,83 % | 3,3918 | 5,2958 |
| ISI-Event-Konstante | 95,83 % | 3,3918 | 5,2958 |

Die Projektion ist dynamisch und effizienter als ihre Event-Konstante, aber
nicht besser als alle einfachen Kontrollen. Der scheinbare ISI-Vorteil ist
vollständig durch eine praktisch konstante Dämpfung erklärbar. Details stehen
in `09_feature_projection/feature_projection_report.md`.

# 13. Forschungsstufe 4: konfigurierbares neurodynamisches System

Die C++-Engine und UI enthalten nun:

- editierbare Membran-, Synapsen-, Adaptations-, STDP-, Stimulus- und
  Readoutparameter,
- individuelle, seed-deterministische Axonverzögerungen je Verbindung,
- wahlweise strombasierte oder AMPA-/GABA-leitwertbasierte Synapsen,
- ein passives Dendritenkompartiment je Neuron mit Soma-Dendrit-Kopplung,
- getrennte Operatorrollen für `E→E`, `E→I`, `I→E`, `I→I`,
- automatische Mehrseed-Auswertung,
- zweiseitige gepaarte Sign-Flip-Permutationstests,
- automatische Projektionsoptimierung auf Trainings-Seeds mit einem
  unberührten Holdout-Seed.

Die historischen strombasierte Ein-Schritt-Referenz bleibt über die UI
reproduzierbar. Der neue erweiterte UI-Standard verwendet AMPA/GABA,
Axonverzögerungen von `1...5 ms`, das passive Dendritenkompartiment und die
Vier-Feature-Projektion.

Diese Mechanismen machen das System zu einem funktionierenden,
ereignisgetriebenen neurodynamischen Computermodell. Sie belegen weder eine
vollständige biologische Gleichwertigkeit noch die Simulation eines konkreten
Organismus.

# 14. Bestätigender Mehrseed-Überlegenheitstest

Vor der neuen Ausführung wurden 24 bisher unberührte Seeds, vier
Belastungsbedingungen, alle sechs Kontrollen und die Entscheidungskriterien
unter `11_superiority_multiseed/experiment_plan.md` festgeschrieben.

Der Lauf umfasst:

```text
24 Seeds × 4 Bedingungen × 6 Modi = 576 Auswertungen
```

Experiment-Hash:

```text
3974457A05BA475D
```

| Modus | Accuracy | Spikes/korrekte Entscheidung |
|---|---:|---:|
| Originalkernel | 0,904297 | 8,334188 |
| event-gematchte Konstante | 0,904297 | 8,391204 |
| deaktiviert | 0,905924 | 8,864264 |
| Vorzeichen | 0,904622 | 8,316918 |
| Tanh | 0,904948 | 8,439888 |
| verteilungsgematchter Zufall | 0,900391 | 8,408236 |

Eine reine Accuracy-Überlegenheit wurde nicht bestätigt. Die
Nichtunterlegenheitsgrenze von `−0,02` wurde jedoch eingehalten, während der
Kernel folgende Holm-korrigierte Spikekostenvorteile erreichte:

- gegenüber Konstante: `0,057016`, `p = 0,003033`,
- gegenüber deaktiviert: `0,530076`, `p = 0,000005`,
- gegenüber Zufall: `0,074048`, `p = 0,005814`.

Experimentell gestützte, eng begrenzte Behauptung:

> In dieser synthetischen Aufgabenfamilie benötigt der Originalkernel bei
> nichtunterlegener Accuracy weniger Spikes je korrekter Entscheidung als
> die event-gematchte Konstante, das deaktivierte Gate und das
> verteilungsgematchte Zufallsgate.

Der Originalkernel ist nicht allen Kontrollen überlegen: Das Vorzeichengate
ist im Mittel geringfügig sparsamer. Eine allgemeine, biologische oder
aufgabenübergreifende Überlegenheit ist daher nicht belegt.

Der Lauf wurde vollständig wiederholt; Rohdaten, Statistik und Bericht waren
byte-identisch.

# 15. Unabhängige Replikation auf Delayed XOR

Vor dem ersten Delayed-XOR-Lauf wurden 24 weitere neue Seeds, zwei
Gedächtnislücken, das 32-dimensionale Readout und alle Entscheidungskriterien
unter `12_delayed_xor_replication/experiment_plan.md` festgeschrieben.

Experiment-Hash:

```text
C18E63A3EA120D7C
```

| Modus | Accuracy | Spikes/korrekte Entscheidung |
|---|---:|---:|
| Originalkernel | 0,512153 | 10,176464 |
| event-gematchte Konstante | 0,513021 | 10,154038 |
| deaktiviert | 0,515625 | 10,787853 |
| Vorzeichen | 0,513455 | 10,119236 |
| Tanh | 0,511719 | 10,228720 |
| verteilungsgematchter Zufall | 0,514757 | 10,155229 |

Die Delayed-XOR-Aufgabe wird von keiner Variante zuverlässig gelöst. Alle
Accuracies liegen nahe dem Zufallsniveau von `0,5`.

Der Kernel ist nur gegenüber dem deaktivierten Gate sparsamer
(`0,611388` Spikes/korrekte Entscheidung, Holm-`p = 0,040360`). Gegenüber
Konstante, Zufall und dem vorab als stärkste Sparsamkeitsbaseline benannten
Vorzeichengate ist er geringfügig teurer.

Vorab festgelegte Entscheidung:

```text
NO_EFFICIENCY_REPLICATION
```

Der komplette Lauf wurde wiederholt. Rohdaten, Maschinenbericht und
Ergebnisbericht waren byte-identisch.

Damit bleibt die Effizienzüberlegenheit aus Stufe 14 ein reproduzierbarer,
aber auf die dortige zeitliche Reihenfolgeaufgabenfamilie begrenzter Befund.
Sie generalisiert nach aktueller Evidenz nicht auf Delayed XOR.

# 16. Entwickelter Gedächtnisreadout

Die Seeds der negativen Delayed-XOR-Replikation wurden danach ausschließlich
als Entwicklungsgruppe verwendet. Eine neue Gruppe von 16 Seeds blieb bis
zum Einfrieren des Modells unberührt.

Version 1 mit ausschließlich post-cue gefilterten Zuständen scheiterte:

```text
Accuracy = 0,524740
```

Version 2 ergänzt eine cue-gebundene Eligibility-Memory. Sie speichert einen
aus Spikeaktivität, Soma- und Dendritspannung gebildeten Zustand des ersten
Hinweises, lässt ihn über `50`, `100` und `200 ms` abklingen und koppelt ihn
erst beim zweiten Hinweis an begrenzte Interaktionsmerkmale.

Entwicklung:

```text
Modellhash             EECE7A502A958561
Accuracy               0,906250
mittlere Verzögerung   0,881944
lange Verzögerung      0,930556
```

Eingefrorene Holdoutbestätigung:

```text
Bestätigungshash       992F651C727D3C20
Accuracy               0,892578
untere 95-%-Grenze     0,876953
mittlere Verzögerung   0,871094
lange Verzögerung      0,914062
```

Damit ist experimentell bestätigt, dass das entwickelte System Delayed XOR
auf neuen Netzwerk-Seeds zuverlässig lernt.

Die Kernel-Effizienzüberlegenheit wird dadurch nicht bestätigt. Der Kernel
ist signifikant sparsamer als deaktiviert (`p_Holm = 0,000085`), aber nicht
signifikant sparsamer als Konstante, Tanh oder Zufall. Das Vorzeichengate
benötigt weiterhin geringfügig weniger Spikes.

Der Holdoutlauf wurde vollständig wiederholt; Rohdaten, Statistik und Bericht
waren byte-identisch.

# 17. Einzelablationen der Gedächtnisstufe

Eligibility-Zeiten sowie Schalter für Eligibility-Memory und
Interaktionsprodukte sind in die native UI übernommen. Der neue
Ablationsbutton berechnet auf identischen Seeds:

| Variante | Accuracy | Spikes/korrekt | Accuracyverlust |
|---|---:|---:|---:|
| Vollmodell | 0,892578 | 5,709169 | 0 |
| ohne Dendrit | 0,785807 | 76,905428 | 0,106771 |
| ohne Eligibility | 0,548828 | 9,380449 | 0,343750 |
| ohne Produkte | 0,475911 | 10,995619 | 0,416667 |
| Vorzeichengate | 0,891927 | 5,704541 | 0,000651 |

Die Kostenunterschiede des Vollmodells gegenüber den drei
Mechanismusablationen sind nach konservativer Holm-Korrektur signifikant.

Experimentell bestätigt ist damit:

- die Eligibility-Memory ist für Delayed XOR notwendig,
- die Interaktionsprodukte sind für die XOR-Nichtlinearität notwendig,
- das Dendritenkompartiment trägt wesentlich zu Leistung und Effizienz bei,
- die Kernelgeometrie ist weiterhin nicht effizienter als das
  Vorzeichengate.

Der Referenzbericht liegt unter
`13_memory_readout_development/ui_ablation_reference/`.

# 18. Lokale Eligibility-Memory pro Synapse

Die Eligibility-Memory wurde zusätzlich als echter Zustand jeder vorhandenen
rekurrenten Verbindung implementiert. Die signierte Spur speichert lokale
Prä-/Post-Kausalität, zerfällt exponentiell und moduliert ausschließlich
spätere Übertragungen derselben Synapse. Nicht vorhandene Verbindungen
behalten exakt den Zustand null.

Die technische Validierung bestätigt:

- deterministische Reproduktion bei identischem Seed,
- exakt unveränderte Dynamik bei `Gain=0`,
- Begrenzung auf das konfigurierte Maximum,
- räumliche Lokalität auf existierende Synapsen,
- endliche Zustände und Erhaltung des Dale-Prinzips,
- unveränderte historische Referenz bei deaktiviertem Mechanismus.

Die gepaarte 16-Seed-Delayed-XOR-Ablation auf den bereits in Stufe 13
verwendeten Holdout-Seeds mit `tau=100 ms`, `Gain=0,35` und `Maximum=4`
ergab:

| Variante | Accuracy | Spikes/korrekt |
|---|---:|---:|
| Vollmodell mit lokaler Spur | 0,891927 | 5,714066 |
| ohne lokale Synapsenspur | 0,892578 | 5,709169 |
| Vorzeichengate mit lokaler Spur | 0,889974 | 5,715085 |

Die lokale Spur verbessert diese Aufgabe in ihrer ersten Parametrisierung
nicht. Der Kostenunterschied zur Nullvariante ist nicht signifikant
(`p_Holm=1,0`). Die cue-gebundene Eligibility im Readout bleibt daher
weiterhin notwendig.

Dieser Test ist eine Entwicklungsablation, keine neue unabhängige
Bestätigung, weil die Seedgruppe bereits in Stufe 13 ausgewertet worden war.
Die vollständige Ablation wurde wiederholt; Bericht und Rohdaten waren
byte-identisch.

# 19. Forschungsstufe 15: Trace-essential Memory

Die explizite cue-gebundene Readout-Memory und alle Produktmerkmale wurden
entfernt. Zwei energiegleiche frühe Cue-Pulse werden von `400 ms` exakt
reizfreier Verzögerung und einem für alle Klassen identischen Recall-Cue
gefolgt. Das lineare Readout sieht ausschließlich Netzwerkzustände aus den
letzten `60 ms` nach dem Recall.

Eine 125er Suche auf vier Entwicklungsseeds fror folgende Parameter ein:

```text
tau=400 ms; Gain=0,50; Maximum=1
```

Danach wurden zwölf vorher unbenutzte Seeds mit elf vorab definierten
Varianten geprüft. Experimenthash:

```text
688F65FA0F77947C
```

| Variante | Accuracy | Spikes/korrekt |
|---|---:|---:|
| signierte Spur | 0,635417 | 91,534899 |
| ohne Spur | 0,500000 | 122,312500 |
| Gain=0 | 0,500000 | 122,312500 |
| event-gematchte Konstante | 0,500000 | 114,666667 |
| 40-ms-verschobene Spur | 0,807292 | 77,072563 |
| synapsenvertauschte Spur | 0,585938 | 108,192828 |
| verteilungsgematchter Zufall | 0,541667 | 109,103863 |
| nur E→E | 0,622396 | 93,380148 |
| nur I→E | 0,518229 | 120,623914 |

Die vorab definierte Entscheidung ist negativ:

```text
TRACE_ESSENTIAL_MEMORY_NOT_CONFIRMED
```

Die signierte Spur verfehlt die Mindestaccuracy von `0,65` knapp und wird
von der Timingkontrolle übertroffen.

Der explorative mechanistische Befund ist dennoch substanziell: Ohne Spur,
bei `Gain=0` und mit event-gematchter Konstante liegt das System exakt bei
`0,50`. Die lokale 40-ms-Spur erreicht `0,807292`; gepaarter Vorteil
`0,307292`, unkorrigiertes exaktes `p=0,000488`. Damit kann die lokale
Spur-Familie im gehärteten Recall-Aufbau zeitliche Information retten. Ein
Vorteil des ursprünglich signierten Timings ist noch nicht bestätigt.

Der Holdout wurde unverändert wiederholt; die 132 Rohdatenzeilen waren
byte-identisch.

# 20. Nächste konkrete Forschungsentscheidung

Die neue unabhängige Hypothese lautet nun nicht mehr „Spur oder keine Spur“,
sondern „welches kausale Auslesetiming der lokalen Spur ist richtig?“. Die
40-ms-Verzögerung muss als Kandidat eingefroren und gegen mehrere
vorab festgelegte Shiftwerte auf einer dritten, unberührten Seedgruppe
bestätigt werden. Die bisherige signierte Sofortwirkung bleibt Kontrolle.

KEEP_AND_RESEARCH
