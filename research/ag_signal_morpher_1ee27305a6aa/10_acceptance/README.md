# Forschungsabnahme der erweiterten GO-SNN-Stufe

Status: `experimentally_supported` für technische Reproduzierbarkeit im
synthetischen Forschungsmodell.

Die Abnahme wird durch
`06_temporal_classification/ui_cpp/acceptance_tests.cpp` implementiert. Sie
ersetzt keine biologische Validierung und behauptet keinen einzigartigen
Nutzen des generierten Operators.

## Referenzzustände

1. `historical_regression`: Die resetgebundene Kernelwirkung bleibt exakt
   `0.12831112128784755` und reproduziert die event-gematchte Konstante
   bitgleich in Spike- und Spannungstrajektorien.
2. `extended_biophysical_null`: AMPA/GABA, Verzögerungen von `1` bis `5 ms`
   und passiver Dendrit laufen endlich und Dale-konform, während alle vier
   Klassenoperatoren deaktiviert sind.
3. `homogeneous_operator_ecology`: Alle sechs Kontrollmodi werden unter
   derselben erweiterten Architektur ausgewertet.
4. `single_operator_roles`: Je Lauf ist nur eine der Klassen `EE`, `EI`,
   `IE`, `II` kernelmoduliert. Die Spannungstrajektorie wird gegen die
   vollständig deaktivierte Operatorökologie verglichen.
5. `extended_multiseed`: Seeds `11`, `23`, `38`, `53`, `71`; Kernel gegen
   deaktiviert mit exaktem zweiseitigem Sign-Flip-Test.

## Aktueller Befund

Alle technischen Prüfungen bestehen. Im bewusst kleinen
Mehrseed-Abnahmedatensatz beträgt der p-Wert Kernel gegen deaktiviert `1.0`.
Das ist negative Evidenz für einen nachweisbaren Accuracy-Vorteil unter
dieser konkreten Konfiguration. Bei fünf Paaren ist der kleinste mögliche
zweiseitige exakte p-Wert `0.0625`.

## Ausführung

```powershell
cd research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp
build\AGBioNetworkAcceptance.exe build\acceptance_results.json
```

`build_ui.bat` akzeptiert optional einen alternativen Buildordner:

```powershell
build_ui.bat build_verify
```

Das ist nützlich, wenn die reguläre UI gerade läuft und ihre EXE deshalb
gesperrt ist.
