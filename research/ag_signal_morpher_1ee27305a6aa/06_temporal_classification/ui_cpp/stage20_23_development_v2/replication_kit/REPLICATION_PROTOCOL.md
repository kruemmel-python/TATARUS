# Unabhängiges Replikationsprotokoll

1. Repository auf einen zweiten Rechner kopieren.
2. MSVC 2022 Build Tools und optional OpenCL installieren.
3. `run_clean_replication.bat` ausführen.
4. CPU, GPU, Compiler, Betriebssystem und Treiberversion notieren.
5. JSON-Status, Seed-CSVs und Hashmanifest zurückgeben.

Eine lokale Ausführung ist keine unabhängige Replikation. Der Status bleibt deshalb `package_ready`, bis Ergebnisse eines zweiten Rechners importiert wurden.
