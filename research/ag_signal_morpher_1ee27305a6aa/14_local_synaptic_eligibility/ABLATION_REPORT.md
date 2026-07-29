# Delayed-XOR-Einzelablationen

Eligibility-Zeiten: `50;100;200 ms`

Lokale Synapsen-Eligibility: `aktiv`, Parameter `tau=100 ms, gain=0.35, maximum=4`

| Variante | Accuracy | Spikes/korrekt | ΔAccuracy zum Vollmodell | ΔKosten zum Vollmodell | p Kosten (Holm konservativ) |
|---|---:|---:|---:|---:|---:|
| full_kernel | 0.891927 | 5.714066 | 0.000000 | 0.000000 | 1.000000 |
| without_dendrite | 0.770833 | 76.066829 | 0.121094 | 70.352763 | 0.000100 |
| without_eligibility | 0.546875 | 9.454308 | 0.345052 | 3.740242 | 0.000090 |
| without_products | 0.475260 | 11.029877 | 0.416667 | 5.315811 | 0.000070 |
| without_local_synaptic_eligibility | 0.892578 | 5.709169 | -0.000651 | -0.004897 | 1.000000 |
| sign_gate | 0.889974 | 5.715085 | 0.001953 | 0.001019 | 1.000000 |

Positive ΔAccuracy/ΔKosten bedeuten einen Vorteil des Vollmodells. Jede Zeile ändert genau einen Mechanismus.
