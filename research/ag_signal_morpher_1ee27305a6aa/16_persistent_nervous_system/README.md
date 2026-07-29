# TATARUS – A Persistent Synthetic Nervous System (Forschungsstufe 16)

Status: `integrated_experimental`  
Stand: 29. Juli 2026

## Ergebnis

Diese Stufe integriert die vorher getrennten Experimente in einen
kontinuierlich laufenden C++-Nervensystemkern. Ein Aufruf von `step()` setzt
den vorhandenen Zustand fort. Neuronen, Rezeptorleitwerte, Axonereignisse,
synaptische Ressourcen, Eligibility-Spuren, konsolidierte Gewichte,
Neuromodulatoren, Energie, Assemblies und die veränderliche Topologie bleiben
erhalten. Ein Reset findet nur beim expliziten Erzeugen eines neuen Systems
statt.

Die drei geforderten Endproduktteile sind ausführbar vorhanden:

1. `TATARUS.exe` und die C++-Klasse
   `PersistentNervousSystem` bilden den Simulator.
2. `AGNervousSystemLab.exe` führt Mehrseed-Entwicklung, Parameter-Evolution,
   Closed-Loop-Läufe, Schadensinjektion und Artefaktexport aus.
3. `mechanism_library.json` enthält die integrierten Mechanismen mit Formel,
   Einsetzposition, Parameterbereich, Evidenz und Status.

## Implementierte funktionale Schichten

| Schicht | Umsetzung |
|---|---|
| Populationen | Sensorisch, exzitatorisch, inhibitorisch, Kontext, Motorik, Modulation |
| Neuron | Soma, passiver Dendrit, Refraktärphase, Adaptation, individuelle Erregbarkeit |
| Synapse | AMPA, NMDA, GABA-A, GABA-B, Modulation, individuelle Axonverzögerung |
| Kurzzeitgedächtnis | Leitwerte, Spike-Traces, synaptische Ressource und Facilitation |
| Mittelfristiges Gedächtnis | signierte lokale Eligibility-Spur pro Synapse |
| Langzeitgedächtnis | belohnungsmodulierte lokale Gewichtsänderung und Konsolidierung |
| Stabilisierung | Zielratenhomeostase, Energiehaushalt, Gewichtsgrenzen, Dale-Prinzip |
| Struktur | nutzungsabhängiges Pruning und geseedetes koaktivitätsabhängiges Wachstum |
| Selbstorganisation | inkrementelle Assembly-Prototypen aus interner Aktivität |
| Sensorik | rohe Bildereignisse, Audiosamples, Berührung, UTF-8-Bytes, Temperatur, Interozeption |
| Handlung | kontinuierliche Bewegung, Aufmerksamkeit, Lautgebung und Konfidenz |
| Umwelt | kausal geschlossener Wahrnehmung-Handlung-Belohnungs-Kreis |
| Persistenz | binärer V9-Snapshot einschließlich RNG, Axonqueue, Assemblies, Reizphasenakkumulatoren, Reparaturprovenienz, langsamer Aktivitätsbasis und Topologie |

Es gibt absichtlich keinen Tokenizer, kein Vokabular und keine
Embeddingtabelle. Text wird als rohe UTF-8-Byte-/Bit-Ereignisfolge eingespeist
und nutzt denselben Spike- und Assemblyraum wie die anderen Modalitäten.

## Validierung

Die finale V2-Ausführung in `results_v2` ergab:

- 7.500 fortlaufende Simulationsschritte,
- 7.439 Spikes und 49.514 tatsächliche synaptische Übertragungen,
- mittlere Rate 7,997799 Hz bei Zielrate 8,121509 Hz,
- mittlere Energie 0,991972,
- 8 selbstgebildete Assemblies,
- 611 aktive Synapsen nach 10 % Neuronen- und 15 % Synapsenschaden,
- 112 strukturelle Wachstumsereignisse,
- positive kumulative Belohnung von 0,172624 in der Erholungsphase,
- endlicher und Dale-konformer Endzustand.

Der OpenCL-Differenztest lief auf `gfx90c` über 512 Zustände. Die maximale
absolute Abweichung zur CPU-Referenz war 0. Erst dieser getestete
Integrationsschritt ist für OpenCL freigegeben; alle komplexeren Mechanismen
bleiben bis zu eigenen Differenztests CPU-autoritativ.

Automatisierte Pflichtprüfungen:

- gleicher Seed → identische Aktionen und identischer Zustands-Hash,
- Snapshot → exakte Zustands- und Fortsetzungsidentität,
- multimodaler Dauerstrom → Spikes und synaptische Übertragung,
- Plastizität und Schadenslauf → Dale-konform und endlich,
- Mechanismenbibliothek → vollständig exportierbar,
- bestehende Engine- und Akzeptanztests → weiterhin grün.

## Wissenschaftliche Grenze

Das Artefakt ist ein funktionales, synthetisches und selbstregulierendes
simuliertes Nervensystem. Es ist kein Nachweis biologischer Gleichwertigkeit,
kein Bewusstseinsmodell und noch kein Beleg für allgemeine Intelligenz.
Insbesondere wurde im einfachen eindimensionalen Umweltlauf das Ziel nicht
zuverlässig erreicht. Die Erholungsphase zeigt Stabilität und Lernsignale,
nicht bereits überlegene Aufgabenleistung.

Diese historische Einschränkung des Stufe-16-Umweltlaufs bleibt korrekt.
Die spätere Stufe 18 bestätigt davon getrennt eine gezielt definierte
Sensor-Motor-Funktion und deren kausale Reparatur auf 8/8 Holdout-Seeds.

## Ausführen

```powershell
cd research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp
build_endgoal\TATARUS.exe
build_endgoal\AGNervousSystemTests.exe build_endgoal\nervous_system_test_output
build_endgoal\AGNervousSystemOpenClProbe.exe opencl_probe.json
build_endgoal\AGNervousSystemLab.exe ..\..\..\16_persistent_nervous_system\results_new
```

Der öffentliche Integrationspunkt für eine höhere KI ist
`nervous_system.hpp`: externe Systeme liefern fortlaufende `SensorFrame`s,
erhalten `MotorAction`s und dürfen den inneren Zustand nur über die
definierten Lern-, Snapshot- und Schadensschnittstellen verändern.
