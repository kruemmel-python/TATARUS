# Versuchsdefinition

## Zweck

Technische und funktionale Erstprüfung einer lokalen Eligibility-Memory pro
Synapse.

## Technische Nullbedingungen

- Mechanismus deaktiviert: historische Dynamik bleibt unverändert.
- Mechanismus aktiv und `Gain=0`: Spuren entstehen, Dynamik und Gewichte
  bleiben exakt wie ohne Mechanismus.
- Nicht vorhandene Synapsen: Eligibility bleibt exakt null.
- Identischer Seed: Spikes, Spannungen und Eligibility-Endzustände sind
  identisch.
- Alle Werte bleiben endlich und innerhalb von `[-Maximum,+Maximum]`.

## Funktionale Entwicklungsablation

- Aufgabe: entwickeltes Delayed-XOR-Readout aus Stufe 13.
- Seeds: die 16 bereits in Stufe 13 verwendeten Holdout-Seeds.
- Lokale Parameter: `tau=100 ms`, `Gain=0.35`, `Maximum=4`.
- Paarung: identische Seeds und Bedingungen für jede Variante.
- Varianten: Vollmodell, ohne Dendrit, ohne Readout-Eligibility, ohne
  Interaktionsprodukte, ohne lokale Synapsen-Eligibility, Vorzeichengate.

Da die Seedgruppe bereits bekannt ist, dürfen die Ergebnisse nur zur
Entwicklung und nicht als unabhängige Bestätigung interpretiert werden.
